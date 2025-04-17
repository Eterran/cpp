"""
Core XGBoost training pipeline: data loading, preprocessing, hyperparameter tuning, training, and evaluation.
"""
import json
import numpy as np
import pandas as pd
import xgboost as xgb
from cppbacktester_py import Config, DataLoader
from sklearn.model_selection import RandomizedSearchCV
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score

def load_data(config_path: str, use_partial: bool = False, partial_percent: float = 100.0) -> pd.DataFrame:
    config = Config()
    if not config.load_from_file(config_path):
        raise FileNotFoundError(f"Could not load config from {config_path}")
    loader = DataLoader(config)
    bars = loader.load_data(use_partial, partial_percent)
    data = [
        {
            'timestamp': bar.timestamp,
            'open': bar.open,
            'high': bar.high,
            'low': bar.low,
            'close': bar.close,
            'bid': bar.bid,
            'ask': bar.ask,
            'volume': bar.volume,
            'extra_columns': bar.extra_columns
        }
        for bar in bars
    ]
    df = pd.DataFrame(data)
    return df

def preprocess_data(df: pd.DataFrame) -> (np.ndarray, np.ndarray):
    df = df.sort_values('timestamp').reset_index(drop=True)
    if 'extra_columns' in df.columns:
        extra_df = pd.DataFrame(df['extra_columns'].tolist()).add_prefix('extra_')
        df = pd.concat([df.drop(columns=['extra_columns']), extra_df], axis=1)
    df['target'] = df['close'].shift(-1)
    df = df.dropna()
    feature_cols = [c for c in df.columns if c not in ['timestamp', 'target']]
    cat_cols = df[feature_cols].select_dtypes(include=['object', 'string']).columns.tolist()
    if cat_cols:
        df = pd.get_dummies(df, columns=cat_cols, drop_first=True)
        feature_cols = [c for c in df.columns if c not in ['timestamp', 'target']]
    X = df[feature_cols].values
    y = df['target'].values
    return X, y

def optimize_hyperparameters(X: np.ndarray, y: np.ndarray, param_config: dict) -> dict:
    base_model = xgb.XGBRegressor(objective='reg:squarederror', verbosity=0)
    param_dist = param_config if param_config else {
        'n_estimators': [100, 200, 500],
        'max_depth': [3, 4, 6, 8],
        'learning_rate': [0.01, 0.05, 0.1, 0.2],
        'subsample': [0.6, 0.8, 1.0],
        'colsample_bytree': [0.6, 0.8, 1.0]
    }
    search = RandomizedSearchCV(
        estimator=base_model,
        param_distributions=param_dist,
        n_iter=20,
        scoring='neg_mean_squared_error',
        cv=3,
        verbose=1,
        random_state=42
    )
    search.fit(X, y)
    return search.best_params_

def train_model(X: np.ndarray, y: np.ndarray, params: dict) -> xgb.XGBRegressor:
    model = xgb.XGBRegressor(objective='reg:squarederror', **params)
    model.fit(X, y)
    return model

def evaluate_model(model: xgb.XGBRegressor, X_test: np.ndarray, y_test: np.ndarray):
    preds = model.predict(X_test)
    mse = mean_squared_error(y_test, preds)
    rmse = np.sqrt(mse)
    r2 = r2_score(y_test, preds)
    metrics = {'rmse': float(rmse), 'r2': float(r2)}
    print(f"Evaluation results:\n  RMSE: {metrics['rmse']:.4f}\n  R2 Score: {metrics['r2']:.4f}")
    return metrics

def save_model(model: xgb.XGBRegressor, path: str) -> None:
    """
    Save trained XGBoost model to disk in JSON or binary format.
    """
    model.save_model(path)

def load_model(path: str) -> xgb.XGBRegressor:
    """
    Load an XGBoost model from disk and return the regressor instance.
    """
    model = xgb.XGBRegressor()
    model.load_model(path)
    return model

def save_metrics(metrics: dict, path: str) -> None:
    """
    Save evaluation metrics to a JSON file for record-keeping.
    """
    with open(path, 'w') as f:
        json.dump(metrics, f, indent=2)
