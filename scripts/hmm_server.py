import sys
import pickle
import numpy as np
import os
import socket
import time
import json

# Load the model once at startup
print("Loading HMM model...", file=sys.stderr)
model_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
                         "hmm_saved", "hmm_model.pkl")
try:
    with open(model_path, "rb") as f:
        model = pickle.load(f)
    print(f"Model loaded successfully from {model_path}", file=sys.stderr)
    print(f"Model type: {type(model)}", file=sys.stderr)
    
    # For HMM model in scikit-learn, n_features is the key attribute
    if hasattr(model, 'n_features'):
        n_features = model.n_features
        print(f"Model expects {n_features} features", file=sys.stderr)
    else:
        # If model doesn't have n_features attribute, try to infer it
        n_features = 1  # Default to 1 if we can't determine
        if hasattr(model, 'means_') and hasattr(model.means_, 'shape'):
            n_features = model.means_.shape[1]
            print(f"Inferred {n_features} features from means_ shape", file=sys.stderr)
        print(f"Using {n_features} features", file=sys.stderr)
        
except Exception as e:
    print(f"Error loading model: {str(e)}", file=sys.stderr)
    sys.exit(1)

# Create a socket server
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

# Bind to localhost on port 12345
server_socket.bind(('localhost', 12345))
server_socket.listen(1)
print("Server started, waiting for connections...", file=sys.stderr)

while True:
    # Accept a connection
    client_socket, addr = server_socket.accept()
    print(f"Connection from {addr}", file=sys.stderr)
    
    try:
        # Get the data
        data = client_socket.recv(4096).decode('utf-8').strip()
        if not data:
            print("No data received, closing connection", file=sys.stderr)
            client_socket.close()
            continue
            
        print(f"Received data: {data}", file=sys.stderr)
        
        # Parse the features from the space-separated string
        features = np.array([float(x) for x in data.split()])
        print(f"Parsed features (shape {features.shape}): {features}", file=sys.stderr)
        
        # This is the key fix: HMM typically expects a 2D array where the second dimension
        # is the number of features expected by the model
        
        # Take the first feature only if needed (for a model that expects only 1 feature)
        if n_features == 1:
            features_reshaped = np.array([[features[0]]])
        else:
            # Try to reshape to correct dimension - this is different from our previous approach
            features_reshaped = features.reshape(1, -1)
            # If we have too many features, take only what we need
            if features_reshaped.shape[1] > n_features:
                features_reshaped = features_reshaped[:, :n_features]
            # If we have too few features, pad with zeros
            elif features_reshaped.shape[1] < n_features:
                padding = np.zeros((1, n_features - features_reshaped.shape[1]))
                features_reshaped = np.hstack((features_reshaped, padding))
        
        print(f"Reshaped features: shape={features_reshaped.shape}, data={features_reshaped}", file=sys.stderr)
        
        # Make prediction
        try:
            predicted_states = model.predict(features_reshaped)
            result = str(predicted_states[0])  # Get first prediction as we have only one sample
            print(f"Prediction result: {result}", file=sys.stderr)
        except Exception as e:
            # Special handling for specific error about shape mismatch
            print(f"Prediction error: {str(e)}", file=sys.stderr)
            if "shapes of a (1, 1) and b (5, 1)" in str(e):
                print("Detected specific shape mismatch, trying alternative approach", file=sys.stderr)
                # This suggests the model was trained with a single feature but somehow expects 5 observations
                # Reshape to (5, 1) - one observation per component
                if len(features) >= 5:
                    features_alt = features[:5].reshape(5, 1)
                else:
                    # If we have less than 5 features, pad with zeros
                    features_alt = np.zeros((5, 1))
                    features_alt[:len(features), 0] = features[:min(5, len(features))]
                
                print(f"Alternative shape: {features_alt.shape}", file=sys.stderr)
                predicted_states = model.predict(features_alt)
                result = str(predicted_states[0])  # Use the first prediction
                print(f"Alternative prediction result: {result}", file=sys.stderr)
            else:
                raise  # Re-raise if it's not this specific error
        
        # Send the result back
        client_socket.sendall(result.encode('utf-8'))
        print(f"Sent result: {result}", file=sys.stderr)
    except Exception as e:
        error_msg = f"Error processing request: {str(e)}"
        print(error_msg, file=sys.stderr)
        try:
            client_socket.sendall(f"ERROR: {error_msg}".encode('utf-8'))
        except:
            pass
    finally:
        client_socket.close()