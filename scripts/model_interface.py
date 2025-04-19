from abc import ABC, abstractmethod

class TradingModel(ABC):
    """
    Abstract base class defining the interface for any trading model.
    """
    @abstractmethod
    def fit(self, X, y):
        """Train the model on features X and targets y."""
        pass

    @abstractmethod
    def predict(self, X):
        """Generate predictions for features X."""
        pass

    @abstractmethod
    def save(self, path: str):
        """Persist model to disk at the given path."""
        pass

    @abstractmethod
    def load(self, path: str):
        """Load model from disk at the given path."""
        pass
