# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.

# A simple powershell script to capture a 60 second performance trace

Wpr.exe -start c:\myTraceProfiles\MF_TRACE.WPRP!MediaPerformance -filemode
start-sleep 60
wpr.exe -stop perftrace.etl
