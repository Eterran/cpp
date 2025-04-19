import sys
import pickle
import numpy as np
import os

def main():
    input_data = sys.stdin.read().strip()
    # Split rows on commas
    rows = input_data.split(",")
    # Then split each row on space to get features
    sequence = np.array([[float(x) for x in row.strip().split()] for row in rows])
    
    # Use the absolute path to the model file
    model_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 
                             "hmm_saved", "hmm_model.pkl")
    
    try:
        with open(model_path, "rb") as f:
            model = pickle.load(f)

        predicted_states = model.predict(sequence)
        print(",".join(map(str, predicted_states)))
    except Exception as e:
        print(f"Error: {str(e)}")
        sys.stderr.write(f"Error loading model: {str(e)}\n")


if __name__ == "__main__":
    main()
