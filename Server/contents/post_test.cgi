#!/bin/bash
read -n "$CONTENT_LENGTH" POST_DATA
echo "Content-Type: text/plain"
echo ""
echo "Received: $POST_DATA"