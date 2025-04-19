import json
from pathlib import Path
import pickle
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType
# add path to cppbacktester_y
import sys
from pathlib import Path
lib_path = Path(__file__).parent.parent / 'build' / 'output' / 'lib'
sys.path.append(str(lib_path))

from cppbacktester_py import Config, DataLoader
from xgb_pipeline import load_data, preprocess_data
from hmmlearn.hmm import GaussianHMM

def main():
    # Locate project root and config file
    root = Path(__file__).parent.parent
    config_path = root / 'config.json'
    cfg = json.loads(config_path.read_text())

    # Regime detection settings
    rd = cfg['RegimeDetection']
    params = rd.get('params', {})
    model_path = root / rd['model_path']

    # Load and preprocess data
    df = load_data(str(config_path))
    X, _ = preprocess_data(df)

    # Fit HMM
    hm_model = GaussianHMM(**params)
    hm_model.fit(X)
    regimes = hm_model.predict(X)

    # Save HMM model
    model_path.parent.mkdir(parents=True, exist_ok=True)
    with open(model_path, 'wb') as f:
        pickle.dump(hm_model, f)
    print(f"HMM model saved to {model_path}")

    # Export HMM to ONNX
    onnx_path = root / cfg["Strategy"]["HMMOnnxPath"]
    onnx_path.parent.mkdir(parents=True, exist_ok=True)
    init_type = [("float_input", FloatTensorType([None, X.shape[1]]))]
    onnx_model = convert_sklearn(hm_model, initial_types=init_type)
    with open(onnx_path, "wb") as of:
        of.write(onnx_model.SerializeToString())
    print(f"HMM ONNX exported to {onnx_path}")

    # Optionally save regime labels
    regime_df = df.iloc[:-1].copy()
    regime_df['regime'] = regimes.tolist()
    out_csv = root / rd['model_path']
    out_csv = out_csv.with_suffix('.csv')
    regime_df.to_csv(out_csv, index=False)
    print(f"Regime labels saved to {out_csv}")

if __name__ == '__main__':
    main()
