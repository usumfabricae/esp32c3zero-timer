#!/bin/bash

# AWS IoT Core Configuration Checker
# Run this to diagnose connection issues

CERT_ID="711bdf0bd55393d90d15aaeee149d4c4aeea6f0b95ad66e387f8f34fbad50cd5"
CERT_ARN="arn:aws:iot:eu-central-1:348831852500:cert/$CERT_ID"
THING_NAME="esp32timer-f76c5c"
POLICY_NAME="esp32timer-policy"
REGION="eu-central-1"

echo "=========================================="
echo "AWS IoT Core Configuration Check"
echo "=========================================="
echo ""

# 1. Check Thing
echo "1. Checking if thing exists..."
if aws iot describe-thing --thing-name $THING_NAME 2>/dev/null; then
    echo "✓ Thing exists: $THING_NAME"
else
    echo "✗ Thing NOT found: $THING_NAME"
    echo "  Run: aws iot create-thing --thing-name $THING_NAME"
fi
echo ""

# 2. Check Certificate
echo "2. Checking certificate status..."
CERT_STATUS=$(aws iot describe-certificate --certificate-id $CERT_ID --query 'certificateDescription.status' --output text 2>/dev/null)
if [ "$CERT_STATUS" == "ACTIVE" ]; then
    echo "✓ Certificate is ACTIVE"
elif [ "$CERT_STATUS" == "INACTIVE" ]; then
    echo "✗ Certificate is INACTIVE"
    echo "  Run: aws iot update-certificate --certificate-id $CERT_ID --new-status ACTIVE"
else
    echo "✗ Certificate NOT found or error"
fi
echo ""

# 3. Check Certificate attached to Thing
echo "3. Checking if certificate is attached to thing..."
PRINCIPALS=$(aws iot list-thing-principals --thing-name $THING_NAME --query 'principals' --output text 2>/dev/null)
if echo "$PRINCIPALS" | grep -q "$CERT_ID"; then
    echo "✓ Certificate is attached to thing"
else
    echo "✗ Certificate NOT attached to thing"
    echo "  Run: aws iot attach-thing-principal --thing-name $THING_NAME --principal $CERT_ARN"
fi
echo ""

# 4. Check Policy attached to Certificate
echo "4. Checking if policy is attached to certificate..."
POLICIES=$(aws iot list-attached-policies --target $CERT_ARN --query 'policies[*].policyName' --output text 2>/dev/null)
if echo "$POLICIES" | grep -q "$POLICY_NAME"; then
    echo "✓ Policy is attached to certificate"
else
    echo "✗ Policy NOT attached to certificate"
    echo "  Run: aws iot attach-policy --policy-name $POLICY_NAME --target $CERT_ARN"
fi
echo ""

# 5. Check Policy Document
echo "5. Checking policy document..."
if aws iot get-policy --policy-name $POLICY_NAME >/dev/null 2>&1; then
    echo "✓ Policy exists: $POLICY_NAME"
    echo ""
    echo "Policy document:"
    aws iot get-policy --policy-name $POLICY_NAME --query 'policyDocument' --output text | jq .
else
    echo "✗ Policy NOT found: $POLICY_NAME"
    echo "  Run: aws iot create-policy --policy-name $POLICY_NAME --policy-document file://esp32timer-policy-corrected.json"
fi
echo ""

# 6. Check Endpoint
echo "6. Checking IoT endpoint..."
ENDPOINT=$(aws iot describe-endpoint --endpoint-type iot:Data-ATS --query 'endpointAddress' --output text)
echo "IoT Endpoint: $ENDPOINT"
echo "Config.h should have: #define IOT_ENDPOINT \"$ENDPOINT\""
echo ""

# 7. Summary
echo "=========================================="
echo "Summary"
echo "=========================================="
echo "Thing Name: $THING_NAME"
echo "Certificate ID: $CERT_ID"
echo "Policy Name: $POLICY_NAME"
echo "Region: $REGION"
echo ""
echo "If any checks failed above, run the suggested commands."
echo ""
