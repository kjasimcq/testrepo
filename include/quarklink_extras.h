/**
 * \file quarklink_extras.h
 * \brief This header file contains some more advanced features that the general user
 * doesn't normally need.
 * These extra features are platform-specific and can only be used in distinct situations,
 * that is with certain devices and configuration combinations.
 * 
 * \copyright Copyright (c) 2023 Crypto Quantique Ltd. All rights reserved.
 */

#ifndef _QUARKLINK_EXTRAS_H_
#define _QUARKLINK_EXTRAS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * \brief ESP32 boards with Digital Signature peripheral
 */

/**
 * \brief Initialises the ds_data provided as parameter with the Digital Signature
 * information related to the device.
 * The obtained `ds_data` can be used in the connection configuration, for example:
 * esp_mqtt_client_config_t mqtt_cfg = {
 *     .credentials.authentication = {
 *         .certificate = quarklink->deviceCert,
 *         .ds_data = ds_data
 *     }
 * \note Make sure to allocate the memory necessary to store the `esp_ds_data_ctx_t`
 * before calling this function.
 * \param[out] ds_data the esp_ds_data_ctx_t that needs to be provided as connection configuration
 * \return int 0 for SUCCESS, other for FAILURE
 */
int quarklink_esp32_getDSData(void *ds_data);


#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif // _QUARKLINK_EXTRAS_H_