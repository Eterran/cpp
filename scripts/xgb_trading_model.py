from pathlib import Path
import xgboost as xgb
from model_interface import TradingModel

class XGBoostTradingModel(TradingModel):
    """
    XGBoost implementation of the TradingModel interface.
    """
    def __init__(self, **params):
        # store hyperparameters for model initialization
        self.params = params
        self.model = None

    def fit(self, X, y):
        """Train an XGBoost regressor on the provided data."""
        self.model = xgb.XGBRegressor(objective='reg:squarederror', **self.params)
        self.model.fit(X, y)
        return self

    def predict(self, X):
        """Generate predictions using the trained model."""
        if self.model is None:
            raise ValueError("Model has not been trained. Call fit() first.")
        return self.model.predict(X)

    def save(self, path: str):
        """Persist the underlying XGBoost model to disk."""
        # ensure directory exists
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        self.model.save_model(path)

    def load(self, path: str):
        """Load an XGBoost model from disk into this instance."""
        self.model = xgb.XGBRegressor(objective='reg:squarederror', **self.params)
        self.model.load_model(path)
        return self