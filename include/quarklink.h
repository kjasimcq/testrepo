/**
 * \file quarklink.h
 * \brief Where the QuarkLink magic happens.
 * \copyright Copyright (c) 2023 Crypto Quantique Ltd. All rights reserved.
 */
#ifndef _QUARKLINK_H_
#define _QUARKLINK_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Constants
 */

/** Current QuarkLink version */
extern const char QUARKLINK_VERSION[];

#define QUARKLINK_MAX_SHORT_DATA_LENGTH     (30)
#define QUARKLINK_MAX_URI_LENGTH            (50)
/** Device IDs are always 64-char strings */
#define QUARKLINK_MAX_DEVICE_ID_LENGTH      (65)
#define QUARKLINK_MAX_ENDPOINT_LENGTH       (128)
#define QUARKLINK_MAX_KEY_LENGTH            (256)
#define QUARKLINK_MAX_CSR_LENGTH            (2048)
#define QUARKLINK_MAX_TOKEN_LENGTH          (650)
#define QUARKLINK_MAX_SHORT_CERT_LENGTH     (1500)
#define QUARKLINK_MAX_LONG_CERT_LENGTH      (2048)

/**
 * \brief Quarklink return codes.  
 * Values below 0 are effectively errors,
 * 0 is the general value for "success",
 * values greater than 0 are not necessarily errors.
 */
typedef enum {
    QUARKLINK_ERROR                             = -1,
    QUARKLINK_INVALID_PARAMETER                 = -2,
    QUARKLINK_CACERTS_ERROR                     = -3,
    QUARKLINK_COMMUNICATION_ERROR               = -4,
    QUARKLINK_VALUE_NOT_AVAILABLE               = -5,
    QUARKLINK_NVM_ERROR                         = -6,
    QUARKLINK_NOT_INITIALISED                   = -7,
    QUARKLINK_AZURE_PROVISIONING_ERROR          = -8,
    QUARKLINK_SUCCESS                           = 0,
    QUARKLINK_STATUS_ENROLLED                   = 10,
    QUARKLINK_STATUS_FWUPDATE_REQUIRED          = 11,
    QUARKLINK_STATUS_NOT_ENROLLED               = 12,
    QUARKLINK_STATUS_CERTIFICATE_EXPIRED        = 13,
    QUARKLINK_STATUS_REVOKED                    = 14,
    QUARKLINK_FWUPDATE_WRONG_SIGNATURE          = 20,
    QUARKLINK_FWUPDATE_MISSING_SIGNATURE        = 21,
    QUARKLINK_FWUPDATE_ERROR                    = 22,
    QUARKLINK_FWUPDATE_NO_UPDATE                = 23,
    QUARKLINK_FWUPDATE_UPDATED                  = 24,
    QUARKLINK_FW_UPDATE_WIFI_LOST               = 25,
    QUARKLINK_DEVICE_DOES_NOT_EXIST             = 30,
    QUARKLINK_DEVICE_REVOKED                    = 31,
    QUARKLINK_CONTEXT_NO_CREDENTIALS_STORED     = 40,
    QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED  = 41,
    QUARKLINK_CONTEXT_NOTHING_STORED            = 42,
} quarklink_return_t;


/**
 * \brief Quarklink context. 
 * This struct is ~6.4KB and contains all the resources needed by a user that wants to use Quarklink.
 * All certificates are in PEM format.
 * \note User needs to define a quarklink_context_t variable and initialise it by calling quarklink_init.
 * Calls to QuarkLink APIs populate the struct fields, which the user is free to access. Empty fields are set to zero/NULL.
 * After quarklink_init, the available fields are: rootCert, endpoint, port, deviceID
 * After quarklink_enrol, the additional available fields are: deviceCert, iotHubRootCert, iotHubEndpoint, iotHubPort
 */
typedef struct quarklink_context_t {

    /** Root certificate of the QuarkLink instance */
    char rootCert[QUARKLINK_MAX_SHORT_CERT_LENGTH];
    /** Temporary certificate obtained from QuarkLink, needed to establish mTLS */
    char *tempCert;
    /** QuarkLink instance endpoint */
    char endpoint[QUARKLINK_MAX_ENDPOINT_LENGTH];
    /** QuarkLink instance port */
    uint16_t port;

    /** Unique Device ID, initialised by \ref quarklink_init() */
    char deviceID[QUARKLINK_MAX_DEVICE_ID_LENGTH];
    /** Device certificate, obtained when enrolling with QuarkLink via \ref quarklink_enrol() */
    char deviceCert[QUARKLINK_MAX_LONG_CERT_LENGTH];

    /** DBS token, obtained when enrolling with QuarkLink Database Direct */
    char token[QUARKLINK_MAX_TOKEN_LENGTH];
    /** DBS URI, obtained when enrolling with QuarkLink Database Direct */
    char uri[QUARKLINK_MAX_URI_LENGTH];
    /** DBS Database, obtained when enrolling with QuarkLink Database Direct */
    char database[QUARKLINK_MAX_SHORT_DATA_LENGTH];
    /** DBS DataSource, obtained when enrolling with QuarkLink Database Direct */
    char dataSource[QUARKLINK_MAX_SHORT_DATA_LENGTH];

    /** IoT Hub root certificate, obtained after enrolling with QuarkLink */
    char iotHubRootCert[QUARKLINK_MAX_LONG_CERT_LENGTH];
    /** IoT Hub endpoint, obtained after enrolling with Quarklink */
    char iotHubEndpoint[QUARKLINK_MAX_ENDPOINT_LENGTH];
    /** IoT Hub port, obtained after enrolling with Quarklink */
    uint16_t iotHubPort;
    /** Scope ID, only applicable when using Azure Device Provisioning Service */
    char *scopeID;

    /** Topic to subscribe to in order to receive firmware update notifications */
    char *fwUpdateTopic;

} quarklink_context_t;

// TODO Do we even need get functions now?
// Leaving them in for future purposes. They do still work anyway, it is just easier 
// for a user to access the struct members directly 
// (although not as safe as there's no check they've been initialised).


/**
 * \brief     	Initialise the Quarklink environment.
 *              
 * \note 		This function needs to be called before any other QuarkLink API. 
 * \param[in,out] quarklink the QuarkLink context to initialise. \note This structure is erased before initialisation.
 * \param[in] 	endpoint    the name of the QuarkLink instance (e.g. cryptoquantique.quarklink.io)
 * \param[in] 	port        the port for the QuarkLink instance connection (normally 6000)
 * \param[in] 	rootCert    the root certificate for the QuarkLink instance
 * \retval      QUARKLINK_SUCCESS for success
 * \retval      Others for error
 */
quarklink_return_t quarklink_init(quarklink_context_t *quarklink, const char* endpoint, uint16_t port, const char* rootCert);

/**
 * \brief     	Enrol with Quarklink to provision and get credentials.
 * \note 		None.
 * \param[in,out] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \retval      QUARKLINK_SUCCESS for success
 * \retval      QUARKLINK_DEVICE_DOES_NOT_EXIST
 * \retval      QUARKLINK_DEVICE_REVOKED
 * \retval      QUARKLINK_CACERTS_ERROR
 * \retval      Other errors
 *
 */
quarklink_return_t quarklink_enrol(quarklink_context_t *quarklink);

/**
 * \brief     	Request the current status of the device to Quarklink
 * \note 		None.
 * \param[in,out] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \return      the current status of the Quarklink device or errors
 * \retval      QUARKLINK_STATUS_ENROLLED
 * \retval      QUARKLINK_STATUS_FWUPDATE_REQUIRED
 * \retval      QUARKLINK_STATUS_NOT_ENROLLED
 * \retval      QUARKLINK_STATUS_CERTIFICATE_EXPIRED
 * \retval      QUARKLINK_STATUS_REVOKED
 * \retval      Other errors
 */
quarklink_return_t quarklink_status(quarklink_context_t *quarklink);

/**
 * \brief       Request a firmware update to QuarkLink, then runs the firmware over the air update.
 * \note        In case of a successful firmware update the device might restart
 *              before the function actually returns something.
 *              The key provided is used for validating the firmware and must match
 *              the key used to sign the firmware by QuarkLink.
 * \param[in,out] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param       signingKey the public key used to validate the firmware, in PEM format. NULL for no key or 
 *              if the key was provisioned using QuarkLink provisioning task.
 * \return      The outcome of the update request
 * \retval      QUARKLINK_FWUPDATE_UPDATED
 * \retval      QUARKLINK_FWUPDATE_NO_UPDATE
 * \retval      QUARKLINK_FWUPDATE_WRONG_SIGNATURE
 * \retval      QUARKLINK_FWUPDATE_MISSING_SIGNATURE
 * \retval      QUARKLINK_FWUPDATE_ERROR
 * \retval      Other errors
 */
quarklink_return_t quarklink_firmwareUpdate(quarklink_context_t *quarklink, const char *signingKey);

/**
 * \brief   Get the unique device ID.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_DEVICE_ID_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the device ID will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getDeviceID(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the device certificate.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_CERTIFICATE_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the device certificate will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getDeviceCert(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the configured QuarkLink root certificate.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_CERTIFICATE_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the root certificate will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getRootCert(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the configured QuarkLink endpoint.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_ENDPOINT_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 *          The endpoint is configured in the form of `instance.quarklink.io`.
 * \param[out] buffer   the buffer where the endpoint will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getEndpoint(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the configured QuarkLink port.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] port the variable where the port value will be copied
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getPort(const quarklink_context_t *quarklink, uint16_t *port);

/**
 * \brief   Get the configured IoT Hub root certificate.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_CERTIFICATE_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the root certificate will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getIoTHubCert(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the configured Iot Hub endpoint.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_ENDPOINT_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the endpoint will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getIoTHubEndpoint(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the configured IoT Hub port.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] port the variable where the port value will be copied
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getIoTHubPort(const quarklink_context_t *quarklink, uint16_t *port);

/**
 * \brief   Get the complete QuarkLink URL, in the form of 'https://endpoint:port'.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_ENDPOINT_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the URL will be saved
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getURL(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the enrolment private key in PEM format.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_KEY_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the key will be saved
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getEnrolmentKey(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the device private key in PEM format.
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_KEY_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the key will be saved
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getDeviceKey(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Get the temporary QuarkLink certificate (PEM format).
 * \note    The buffer provided needs to be at least `QUARKLINK_MAX_CERTIFICATE_LENGTH` bytes.
 * \param[in] quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[out] buffer   the buffer where the certificate will be copied
 * \param[in]  length   the size of the buffer provided
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 * \retval QUARKLINK_VALUE_NOT_AVAILABLE
 */
quarklink_return_t quarklink_getTempCert(const quarklink_context_t *quarklink, char *buffer, int length);

/**
 * \brief   Set the root certificate. Use to update if it changed after quarklink_init was called.
 * \note    
 * \param[in,out]   quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[in]       rootCert  the new root certificate
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 */
quarklink_return_t quarklink_setRootCert(quarklink_context_t *quarklink, const char *rootCert);

/**
 * \brief   Set the endpoint. Use to update if it changed after quarklink_init was called.
 * \note    
 * \param[in,out]   quarklink the QuarkLink context, make sure to initialise it with \ref quarklink_init() first
 * \param[in]       endpoint  the new root certificate
 * \param[in]       port      the new port
 * \retval QUARKLINK_SUCCESS
 * \retval QUARKLINK_INVALID_PARAMETER
 */
quarklink_return_t quarklink_setEndpoint(quarklink_context_t *quarklink, const char *endpoint, uint16_t port);

/**
 * \brief Persist the current context to non-volatile storage. Call this function to store the QuarkLink details.
 * Use `quarklink_loadStoredContext()` to load the persisted context.
 * QuarkLink details include rootCert, endpoint and port.
 * 
 * \param[in]   quarklink the context to be stored. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_NOT_INITIALISED
 * \retval  QUARKLINK_NVM_ERROR
 */
quarklink_return_t quarklink_persistContext(const quarklink_context_t *quarklink);

/**
 * \brief Persist the current context to non-volatile storage. Call this function after a successful enrol
 * to avoid re-enrolling the device every time it restarts.
 * Use `quarklink_loadStoredContext()` to load the persisted context.
 * Enrolment context includes: deviceCert, IoT Hub endpoint, port and certificate, scopeID, firmware update topic.
 * 
 * \param[in]   quarklink the context to be stored. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_NOT_INITIALISED
 * \retval  QUARKLINK_NVM_ERROR
 */
quarklink_return_t quarklink_persistEnrolmentContext(const quarklink_context_t *quarklink);

/**
 * \brief Load the QuarkLink context saved in non-volatile storage. 
 * Call this function to load the last configuration, to avoid re-enrolling the device after each restart.
 * Use `quarklink_persistContext()` to persist the context.
 * 
 * \param[out]  quarklink the context to be updated with the stored configuration. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_NOT_INITIALISED
 * \retval  QUARKLINK_CONTEXT_NO_CREDENTIALS_STORED
 * \retval  QUARKLINK_CONTEXT_NO_ENROLMENT_INFO_STORED
 * \retval  QUARKLINK_CONTEXT_NOTHING_STORED
 * \retval  QUARKLINK_NVM_ERROR
 */
quarklink_return_t quarklink_loadStoredContext(quarklink_context_t *quarklink);

/**
 * \brief Only delete the enrollment details from the context saved with `quarklink_persistContext()`.
 * Enrollment details include: deviceCert, IoT Hub endpoint, port and certificate, scopeID, firmware update topic.
 * 
 * \param[in]  quarklink the context to be deleted. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_ERROR
 */
quarklink_return_t quarklink_deleteEnrolmentContext(const quarklink_context_t *quarklink);

/**
 * \brief Delete the QuarkLink context saved with `quarklink_persistContext()`.
 * Quarklink context includes: rootCert, endpoint and port.
 * 
 * \param[in]  quarklink the context to be deleted. Needs to be initialised first.
 * \retval  QUARKLINK_SUCCESS
 * \retval  QUARKLINK_ERROR
 */
quarklink_return_t quarklink_deleteContext(const quarklink_context_t *quarklink);

/**
 * \brief Check if the device is currently enrolled.
 * 
 * This function needs to be called after `quarklink_status()`, in order to 
 * have an up-to-date device status.
 * \see quarklink_status
 * 
 * \note `quarklink_isDeviceEnrolled()`, `quarklink_isDeviceNotEnrolled()`, 
 * `quarklink_isDeviceRevoked()` and `quarklink_isDevicePendingRevoke()` are mutually
 * exclusive, meaning only one of these can be true at the same time.
 * 
 * \return 1 if true, 0 if false
 */
int quarklink_isDeviceEnrolled();

/**
 * \brief Check if the device is currently not enrolled.
 * 
 * This function needs to be called after `quarklink_status()`, in order to 
 * have an up-to-date device status.
 * \see quarklink_status
 * 
 * \note `quarklink_isDeviceEnrolled()`, `quarklink_isDeviceNotEnrolled()`, 
 * `quarklink_isDeviceRevoked()` and `quarklink_isDevicePendingRevoke()` are mutually
 * exclusive, meaning only one of these can be true at the same time.
 * 
 * \return 1 if true, 0 if false
 */
int quarklink_isDeviceNotEnrolled();

/**
 * \brief Check if the device is currently revoked.
 * 
 * This function needs to be called after `quarklink_status()`, in order to 
 * have an up-to-date device status.
 * \see quarklink_status
 * 
 * \note `quarklink_isDeviceEnrolled()`, `quarklink_isDeviceNotEnrolled()`, 
 * `quarklink_isDeviceRevoked()` and `quarklink_isDevicePendingRevoke()` are mutually
 * exclusive, meaning only one of these can be true at the same time.
 * 
 * \return 1 if true, 0 if false
 */
int quarklink_isDeviceRevoked();

/**
 * \brief Check if the device is currently pending on a revoke.
 * 
 * This function needs to be called after `quarklink_status()`, in order to 
 * have an up-to-date device status.
 * \see quarklink_status
 * 
 * \note `quarklink_isDeviceEnrolled()`, `quarklink_isDeviceNotEnrolled()`, 
 * `quarklink_isDeviceRevoked()` and `quarklink_isDevicePendingRevoke()` are mutually
 * exclusive, meaning only one of these can be true at the same time.
 * 
 * \return 1 if true, 0 if false
 */
int quarklink_isDevicePendingRevoke();

/**
 * \brief Check if the device certificate is expired.
 * 
 * This function needs to be called after `quarklink_status()`, in order to 
 * have an up-to-date device status.
 * \see quarklink_status
 * 
 * \note `quarklink_isDeviceEnrolled()`, `quarklink_isDeviceNotEnrolled()`, 
 * `quarklink_isDeviceRevoked()` and `quarklink_isDevicePendingRevoke()` are mutually
 * exclusive, meaning only one of these can be true at the same time.
 * 
 * \return 1 if true, 0 if false
 */
int quarklink_isDeviceCertificateExpired();

/**
 * \brief Check if there is a firmware update available for the device.
 * 
 * This function needs to be called after `quarklink_status()`, in order to 
 * have an up-to-date device status.
 * \see quarklink_status
 * 
 * \note `quarklink_isDeviceEnrolled()`, `quarklink_isDeviceNotEnrolled()`, 
 * `quarklink_isDeviceRevoked()` and `quarklink_isDevicePendingRevoke()` are mutually
 * exclusive, meaning only one of these can be true at the same time.
 * 
 * \return 1 if true, 0 if false
 */
int quarklink_isDeviceFwUpdateAvailable();

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif // _QUARKLINK_H_
