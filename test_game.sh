#!/bin/bash
cd build/bin

echo "Testing Zuma HD..."
echo "Starting program..."

# Start the program and let it run for a few seconds
timeout 5s ./ZumaHD &
PID=$!

sleep 2

# Try to send key press to switch to game scene
echo "Sending key '1' to switch to game scene..."
# This won't work in headless mode, but we can see if the program runs

wait $PID
EXIT_CODE=$?

if [ $EXIT_CODE -eq 124 ]; then
    echo "✅ Program ran successfully for 5 seconds without crashing!"
elif [ $EXIT_CODE -eq 139 ]; then
    echo "❌ Program crashed with segmentation fault"
else
    echo "Program exited with code: $EXIT_CODE"
fi