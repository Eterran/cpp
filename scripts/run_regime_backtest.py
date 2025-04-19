import json
from pathlib import Path
import pickle
import numpy as np

from xgb_pipeline import load_data, preprocess_data
from model_interface import TradingModel

# Assume train_regime_models has already saved models and the HMM

def main():
    root = Path(__file__).parent.parent
    cfg = json.loads((root / 'config.json').read_text())

    # Load HMM for regime prediction
    rd = cfg['RegimeDetection']
    with open(root / rd['model_path'], 'rb') as f:
        hmm_model = pickle.load(f)

    # Load and preprocess data
    df = load_data(str(root / 'config.json'))
    X, _ = preprocess_data(df)

    # Predict regimes on full dataset
    regimes = hmm_model.predict(X)

    # Load all trained models
    model_registry = {}
    for spec in cfg['Models']:
        name = spec['name']
        model_path = root / spec['model_path']
        # Instantiate based on spec type
        if spec['type'] == 'XGBoost':
            from xgb_trading_model import XGBoostTradingModel
            m = XGBoostTradingModel(**spec.get('hyperparams', {}))
            m.load(str(model_path))
        else:
            continue
        model_registry[name] = (m, spec['regimes'])

    # Generate predictions per timestamp
    predictions = []
    for xi, regime in zip(X, regimes):
        # Find the model that covers this regime
        for name, (model, regimes_) in model_registry.items():
            if regime in regimes_:
                pred = model.predict(xi.reshape(1, -1))[0]
                predictions.append({'timestamp_index': len(predictions), 'regime': int(regime), 'model': name, 'prediction': float(pred)})
                break

    # Output predictions
    out_path = root / 'scripts' / 'regime_predictions.json'
    with open(out_path, 'w') as f:
        json.dump(predictions, f, indent=2)
    print(f"Saved regime-based predictions to {out_path}")

if __name__ == '__main__':
    main()