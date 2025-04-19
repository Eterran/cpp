@echo off
echo Starting HMM Python Server...
start /b python E:/CPP/v3/scripts/hmm_server.py > hmm_server_log.txt 2>&1
echo Server started in background. See hmm_server_log.txt for logs.
ping -n 3 127.0.0.1 > nul
echo Done.