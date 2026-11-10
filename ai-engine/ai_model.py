import firebase_admin
from firebase_admin import credentials, db
import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestRegressor
import schedule
import time
from datetime import datetime
import os

# 1. Initialize Firebase Admin
# Place your serviceAccountKey.json in the same folder as this script
KEY_PATH = "serviceAccountKey.json"

if not os.path.exists(KEY_PATH):
    print(f"[ERROR] '{KEY_PATH}' not found in the current folder!")
    print("Please download your service account private key from Firebase Console -> Project Settings -> Service Accounts.")
    exit(1)

cred = credentials.Certificate(KEY_PATH)
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://abdalrhman-mostafa-default-rtdb.firebaseio.com/'
})

# 2. Fetch the latest 50 readings from the ESP32
def fetchData():
    ref = db.reference('/readings')
    data = ref.order_by_key().limit_to_last(50).get()
    
    if not data:
        print("[AI] No data in Firebase yet. Waiting for ESP32 readings...")
        return None
        
    records = []
    for key, val in data.items():
        if isinstance(val, dict):
            records.append({
                'moisture': val.get('moisture', 0),
                'tds': val.get('tds', 0),
                'relay': 1 if val.get('relay') == 'ON' else 0,
                'mode': val.get('mode', 'Auto'),
                'timestamp': val.get('timestamp', 0)
            })
            
    df = pd.DataFrame(records)
    df = df.sort_values('timestamp').reset_index(drop=True)
    print(f"[AI] Loaded {len(df)} records from Firebase.")
    return df

# 3. Feature Engineering for Machine Learning
def buildFeatures(df):
    features = pd.DataFrame()
    features['moisture_now'] = df['moisture']
    features['moisture_avg5'] = df['moisture'].rolling(5, min_periods=1).mean()
    features['moisture_diff'] = df['moisture'].diff().fillna(0)
    features['relay_count10'] = df['relay'].rolling(10, min_periods=1).sum()
    return features

# 4. Predict the Next Moisture Reading using Random Forest
def predictNextMoisture(df):
    if len(df) < 5:
        print("[AI] Not enough data points yet (need at least 5 readings).")
        return None
        
    X = buildFeatures(df).iloc[:-1]       # All rows except the last
    y = df['moisture'].iloc[1:].values    # Target is the subsequent reading
    
    model = RandomForestRegressor(n_estimators=30, max_depth=4, random_state=42)
    model.fit(X, y)
    
    lastFeatures = buildFeatures(df).iloc[[-1]]
    predicted = model.predict(lastFeatures)[0]
    predicted = max(0, min(100, round(predicted, 1)))
    return predicted

# 5. Predict Hours Until Soil Becomes Dry (< 30%)
def predictHoursUntilDry(df):
    currentMoisture = df['moisture'].iloc[-1]
    avgDrop = df['moisture'].diff().mean()
    
    if avgDrop >= 0:
        return -1  # Moisture is stable or rising
    else:
        # Assuming reading interval ~60 seconds
        readingsUntilDry = (currentMoisture - 30) / abs(avgDrop)
        hoursUntilDry = round((readingsUntilDry * 60) / 3600, 1)
        return max(0.1, hoursUntilDry)

# 6. Detect Sensor Anomalies (Faulty, stuck, or out-of-range sensor)
def detectSensorAnomaly(df):
    recent = df['moisture'].tail(10)
    
    if recent.std() < 0.2 and len(recent) >= 5:
        return True, "Sensor stuck: values are completely frozen."
    if recent.diff().abs().max() > 35:
        return True, "Unnatural moisture spike detected."
    if recent.min() < 3 or recent.max() > 98:
        return True, "Moisture value out of normal physical range."
        
    return False, "Sensor operating normally."

# 7. Push Results to Firebase
def savePredictions(nextMoisture, hoursUntilDry, anomaly, anomalyMsg):
    ref = db.reference('/predictions')
    ref.set({
        'next_moisture': float(nextMoisture) if nextMoisture is not None else -1,
        'hours_until_dry': float(hoursUntilDry) if hoursUntilDry is not None else -1,
        'need_irrigation': bool(hoursUntilDry != -1 and hoursUntilDry < 2),
        'sensor_anomaly': bool(anomaly),
        'anomaly_message': str(anomalyMsg),
        'last_updated': datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        'status': 'ok'
    })
    print("[AI] Predictions published to Firebase successfully! ✓")

# 8. Main Execution Function
def runAI():
    print(f"\n==============================================")
    print(f"[AI] Running at {datetime.now().strftime('%H:%M:%S')}")
    df = fetchData()
    if df is None or len(df) < 5:
        return
        
    nextMoisture = predictNextMoisture(df)
    hoursUntilDry = predictHoursUntilDry(df)
    anomaly, msg = detectSensorAnomaly(df)
    
    print(f" -> Predicted Next Moisture: {nextMoisture}%")
    print(f" -> Hours Until Dry: {hoursUntilDry} hrs")
    print(f" -> Sensor Anomaly: {anomaly} ({msg})")
    
    savePredictions(nextMoisture, hoursUntilDry, anomaly, msg)

if __name__ == "__main__":
    print("[AI] Smart Irrigation Engine Started.")
    runAI()
    schedule.every(1).minutes.do(runAI)
    
    while True:
        schedule.run_pending()
        time.sleep(1)
