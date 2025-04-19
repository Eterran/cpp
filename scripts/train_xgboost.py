"""
Train XGBoost model on backtester data.
"""
# add path to cppbacktester_y
import sys
from pathlib import Path
lib_path = Path(__file__).parent.parent / 'build' / 'output' / 'lib'
sys.path.append(str(lib_path))

import argparse
import json
import numpy as np
import pandas as pd
import xgboost as xgb
from cppbacktester_py import Config, DataLoader
from sklearn.model_selection import train_test_split
# Import pipeline functions for cleaner entry script
from xgb_pipeline import load_data, preprocess_data, optimize_hyperparameters, train_model, evaluate_model, save_model, save_metrics

def main():
    """
    Main function to coordinate data loading, preprocessing, optimization, training, and evaluation.
    """
    parser = argparse.ArgumentParser(description="Train XGBoost model with backtester data.")
    parser.add_argument("--config", type=str, default=str(Path(__file__).parent.parent / 'config.json'), help="Path to config file.")
    parser.add_argument("--partial", action="store_true", help="Use partial data load.")
    parser.add_argument("--partial_percent", type=float, default=100.0, help="Percent of data to load if partial.")
    parser.add_argument("--test_size", type=float, default=0.2, help="Fraction of data to reserve for testing.")
    parser.add_argument("--random_state", type=int, default=42, help="Random seed for data splitting and CV.")
    parser.add_argument("--param_config", type=str, default=None, help="Path to JSON file with hyperparameter search spaces.")
    parser.add_argument("--model_output", type=str,
        default=str(Path(__file__).parent / "xgb_saved" / "xgb_model.json"),
        help="Path to save trained model.")
    parser.add_argument("--metrics_output", type=str,
        default=str(Path(__file__).parent / "xgb_saved" / "metrics.json"),
        help="Path to save evaluation metrics.")

    args = parser.parse_args()

    # Load and prepare data
    df = load_data(args.config, use_partial=args.partial, partial_percent=args.partial_percent)
    X, y = preprocess_data(df)

    # Split into train/test
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=args.test_size, random_state=args.random_state)

    # Load hyperparameter search space if provided
    param_space = {}
    if args.param_config:
        with open(args.param_config, 'r') as f:
            param_space = json.load(f)

    # Hyperparameter optimization
    best_params = optimize_hyperparameters(X_train, y_train, param_space)
    print(f"Best parameters: {best_params}")

    # Training
    model = train_model(X_train, y_train, best_params)

    # Evaluation and save metrics
    metrics = evaluate_model(model, X_test, y_test)

    # Persist model and metrics
    # Ensure output directories exist
    Path(args.model_output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.metrics_output).parent.mkdir(parents=True, exist_ok=True)
    save_model(model, args.model_output)
    print(f"Model saved to {args.model_output}")
    save_metrics(metrics, args.metrics_output)
    print(f"Metrics saved to {args.metrics_output}")

if __name__ == "__main__":
    main()
