#ifndef PROVISION_CERTS_H
#define PROVISION_CERTS_H

/**
 * AWS IoT Core Certificates for Device Provisioning
 * 
 * SECURITY WARNING: This file contains sensitive credentials!
 * - Do NOT commit this file to version control
 * - Remove this file after provisioning is complete
 * - For production, use a secure provisioning process
 * 
 */

// Device Certificate (PEM format)
const char *DEVICE_CERT = 
"-----BEGIN CERTIFICATE-----\n"
"MII.....\n"
"-----END CERTIFICATE-----\n";

// Private Key (PEM format)
const char *PRIVATE_KEY = 
"-----BEGIN RSA PRIVATE KEY-----\n"
"MII.....\n"
"-----END RSA PRIVATE KEY-----\n";

// AWS Root CA Certificate (PEM format)
const char *ROOT_CA = 
"-----BEGIN CERTIFICATE-----\n"
"MII.....\n"
"-----END CERTIFICATE-----\n";

#endif // PROVISION_CERTS_H
