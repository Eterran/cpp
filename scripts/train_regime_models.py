import sys
from pathlib import Path
# ensure C++ extension library is on Python path
lib_path = Path(__file__).parent.parent / 'build' / 'output' / 'lib'
sys.path.append(str(lib_path))

import json
import pickle
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score
from skl2onnx import convert_sklearn
from skl2onnx.common.data_types import FloatTensorType

from xgb_pipeline import load_data, preprocess_data
from model_interface import TradingModel
from xgb_trading_model import XGBoostTradingModel

# Map model types to TradingModel classes
MODEL_REGISTRY = {
    "XGBoost": XGBoostTradingModel,
    # future: "HMM": HMMTradingModel,
}

def main():
    # locate project root and config
    root = Path(__file__).parent.parent
    cfg_path = root / 'config.json'
    cfg = json.loads(cfg_path.read_text())

    # load and preprocess data
    df = load_data(str(cfg_path))
    X, y = preprocess_data(df)

    # load and predict regimes
    rd = cfg['RegimeDetection']
    with open(root / rd['model_path'], 'rb') as f:
        hmm_model = pickle.load(f)
    regimes = hmm_model.predict(X)

    # append regime label
    df = df.iloc[:-1].copy()
    df['regime'] = regimes
    # X and y are already aligned with regimes

    # train per-model per-regime
    for spec in cfg.get('Models', []):
        name = spec['name']
        model_type = spec['type']
        ModelClass = MODEL_REGISTRY.get(model_type)
        if ModelClass is None:
            print(f"Skipping unknown model type: {model_type}")
            continue

        # filter by regimes
        mask = df['regime'].isin(spec['regimes'])
        if not mask.any():
            print(f"No data for model {name}, regimes {spec['regimes']}")
            continue
        X_sel = X[mask.values]
        y_sel = y[mask.values]

        # train/test split
        test_size = spec.get('test_size', 0.2)
        random_state = spec.get('random_state', 42)
        X_tr, X_te, y_tr, y_te = train_test_split(
            X_sel, y_sel, test_size=test_size, random_state=random_state
        )

        # instantiate and fit
        model = ModelClass(**spec.get('hyperparams', {}))
        model.fit(X_tr, y_tr)

        # evaluate
        preds = model.predict(X_te)
        mse = mean_squared_error(y_te, preds)
        rmse = np.sqrt(mse)
        r2 = r2_score(y_te, preds)
        print(f"Model {name} evaluation: RMSE={rmse:.4f}, R2={r2:.4f}")
        metrics = {'rmse': float(rmse), 'r2': float(r2), 'n_train': len(y_tr), 'n_test': len(y_te)}

        # save model
        model_path = root / spec['model_path']
        model_path.parent.mkdir(parents=True, exist_ok=True)
        model.save(str(model_path))

        # Export trained XGBoost model to ONNX
        onnx_path = model_path.with_suffix('.onnx')
        init_type = [("float_input", FloatTensorType([None, X_tr.shape[1]]))]
        onnx_model = convert_sklearn(model.model, initial_types=init_type)
        with open(onnx_path, 'wb') as of:
            of.write(onnx_model.SerializeToString())
        print(f"Exported XGBoost ONNX model to {onnx_path}")

        # save metrics
        metrics_path = model_path.with_name(f"metrics_{name}.json")
        with open(metrics_path, 'w') as mf:
            json.dump(metrics, mf, indent=2)
        print(f"Saved {name} to {model_path} and metrics to {metrics_path}")

if __name__ == '__main__':
    main()
