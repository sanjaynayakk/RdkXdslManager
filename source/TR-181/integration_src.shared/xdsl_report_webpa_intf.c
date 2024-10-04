/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2019 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
/**********************************************************************
   Copyright [2014] [Cisco Systems, Inc.]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
**********************************************************************/

#include "../../RdkXdslManager/ssp_global.h"
#include "stdlib.h"
#include <libparodus/libparodus.h>
#include "xdsl_report.h"

static libpd_instance_t client_instance;
extern  ANSC_HANDLE bus_handle;
extern char deviceMAC[32];

#ifdef ENABLE_SESHAT
/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/
static void get_parodus_url(char **url)
{
    char *seshat_url = NULL;
    char *discovered_url = NULL;
    size_t discovered_url_size = 0;

    get_seshat_url(&seshat_url);
    if( NULL == seshat_url ) {
        CcspTraceError((" seshat_url is not present in device.properties file\n"));
        return;
    }

    CcspTraceInfo((" seshat_url formed is %s\n", seshat_url));
    if( 0 == init_lib_seshat(seshat_url) ) {
        CcspTraceInfo((" seshatlib initialized! (url %s)\n", seshat_url));

        discovered_url = seshat_discover(PARODUS_SERVICE);
        if( NULL != discovered_url ) {
            discovered_url_size = strlen(discovered_url);
            *url = strndup(discovered_url, discovered_url_size);
            CcspTraceInfo((" seshatlib discovered url = %s, parodus url = %s\n", discovered_url, *url));
            CcspTraceInfo(("XDSL REPORT %s : LINE %d free()\n", __FUNCTION__,  __LINE__));
            free(discovered_url);
        } else {
            CcspTraceError((" seshatlib registration error (url %s)!", discovered_url));
        }
    } else {
        CcspTraceError((" seshatlib not initialized! (url %s)\n", seshat_url));
    }

    if( NULL == *url ) {
        CcspTraceError((" parodus url (url %s) is not present in seshatlib (url %s)\n", *url, seshat_url));
    }
    CcspTraceInfo((" parodus url formed is %s\n", *url));

    shutdown_seshat_lib();
}

/*----------------------------------------------------------------------------*/
/*                             Internal functions                             */
/*----------------------------------------------------------------------------*/
/**
 * @brief Helper function to retrieve seshat URL.
 * 
 * @param[out] URL.
 *
 */
static void get_seshat_url(char **url)
{
    FILE *fp = fopen(DEVICE_PROPS_FILE, "r");

    if( NULL != fp ) {
        char str[255] = {'\0'};
        while( fscanf(fp,"%s", str) != EOF ) {
            char *value = NULL;
            if( value = strstr(str, "SESHAT_URL=") ) {
                value = value + strlen("SESHAT_URL=");
                *url = strdup(value);
                CcspTraceInfo((" seshat_url is %s\n", *url));
            }
        }
    } else {
        CcspTraceError((" Failed to open device.properties file:%s\n", DEVICE_PROPS_FILE));
    }
    fclose(fp);
}
#else
#define PARODUS_URL_DEFAULT      "tcp://127.0.0.1:6666"
static void get_parodus_url(char **url)
{
    FILE *fp = fopen(DEVICE_PROPS_FILE, "r");

    if( NULL != fp ) {
        char str[255] = {'\0'};
        while( fscanf(fp,"%s", str) != EOF) {
            char *value = NULL;
            if( NULL != (value = strstr(str, "PARODUS_URL=")) ) {
                value = value + strlen("PARODUS_URL=");
                *url = strdup(value);
                CcspTraceInfo((" parodus url is %s\n", *url));
            }
        }
    } else {
        CcspTraceError((" Failed to open device.properties file:%s\n", DEVICE_PROPS_FILE));
        CcspTraceInfo(("Adding default value for parodus_url\n"));
        *url = strdup(PARODUS_URL_DEFAULT);
    }
    fclose(fp);

    if( NULL == *url ) {
        CcspTraceWarning((" parodus url is not present in device.properties file, adding default parodus_url\n"));
        *url = strdup(PARODUS_URL_DEFAULT);
    }

    CcspTraceInfo((" parodus url formed is %s\n", *url));
}
#endif

int ParodusClientInit()
{
    CcspTraceInfo(("XDSL REPORT %s : LINE %d client_instance = %p\n", __FUNCTION__, __LINE__, client_instance));
    int backoffRetryTime = 0;
    int backoff_max_time = 9;
    int max_retry_sleep;
    //Retry Backoff count shall start at c=2 & calculate 2^c - 1.
    int c =2;
    int retval=-1;
    char *parodus_url = NULL;

    CcspTraceInfo((" ******** Start of ParodusClientInit ********\n"));

    CcspTraceInfo(("XDSL REPORT %s : LINE %d client_instance = %p\n", __FUNCTION__, __LINE__, client_instance));

    max_retry_sleep = (int) pow(2, backoff_max_time) -1;
    CcspTraceInfo((" max_retry_sleep is %d\n", max_retry_sleep ));

    CcspTraceInfo((" Call parodus library init api \n"));

    CcspTraceInfo(("XDSL REPORT %s : LINE %d client_instance = %p\n", __FUNCTION__, __LINE__, client_instance));
    get_parodus_url(&parodus_url);
    if(parodus_url == NULL)
    {
        CcspTraceError((" Cannot retrieve parodus url \n"));
        return ANSC_STATUS_FAILURE;
    }
    libpd_cfg_t cfg1 = {.service_name = "xdsl",
        .receive = false, .keepalive_timeout_secs = 0,
        .parodus_url = parodus_url,
        .client_url = NULL
    };

    CcspTraceInfo((" Configurations => service_name : %s parodus_url : %s client_url : %s\n", cfg1.service_name, cfg1.parodus_url, cfg1.client_url ));

    while(1)
    {
        CcspTraceInfo(("XDSL REPORT %s : LINE %d client_instance = %p\n", __FUNCTION__, __LINE__, client_instance));
        if(backoffRetryTime < max_retry_sleep)
        {
            backoffRetryTime = (int) pow(2, c) -1;
        }

        CcspTraceInfo((" New backoffRetryTime value calculated as %d seconds\n", backoffRetryTime));
        int ret =libparodus_init (&client_instance, &cfg1);

        CcspTraceInfo(("XDSL REPORT %s : LINE %d client_instance = %p\n", __FUNCTION__, __LINE__, client_instance));
        CcspTraceInfo((" ret is %d\n",ret));
        if(ret ==0)
        {
            CcspTraceInfo((" Init for parodus Success..!!\n"));
            return ANSC_STATUS_SUCCESS;
        }
        else
        {
            CcspTraceError((" Init for parodus (url %s) failed: '%s'\n", parodus_url, libparodus_strerror(ret)));
            if( NULL == parodus_url )
            {
                get_parodus_url(&parodus_url);
                cfg1.parodus_url = parodus_url;
            }
            sleep(backoffRetryTime);
            c++;
        }
        retval = libparodus_shutdown(client_instance);
        CcspTraceInfo((" libparodus_shutdown retval: %d\n", retval));
    }
    CcspTraceError((" libparodus_init failure \n"));
    return ANSC_STATUS_FAILURE;
}

void sendWebpaMsg(char *serviceName, char *dest, char *trans_id, char *contentType, char *payload, unsigned int payload_len)
{
    wrp_msg_t *wrp_msg ;
    int retry_count = 0, backoffRetryTime = 0, c = 2;
    int sendStatus = -1;
    char source[MAX_PARAMETERNAME_LEN/2] = {'\0'};
    CcspTraceInfo((" XDSL REPORT %s ENTER\n", __FUNCTION__ ));
    CcspTraceInfo((" <======== Start of sendWebpaMsg =======>\n"));
    if(serviceName!= NULL){
        CcspTraceInfo((" serviceName :%s\n",serviceName));
    }
    if(dest!= NULL){
        CcspTraceInfo((" dest :%s\n",dest));
    }
    if(trans_id!= NULL){
        CcspTraceInfo((" trans_id :%s\n",trans_id));
    }
    if(contentType!= NULL){
        CcspTraceInfo((" contentType :%s\n",contentType));
    }
    CcspTraceInfo((" payload_len :%d\n",payload_len));
    if(payload!= NULL){
        CcspTraceInfo((" payload :%s\n",payload));
    }

    snprintf(source, sizeof(source), "mac:%s/%s", deviceMAC, serviceName);
    CcspTraceInfo((" Received DeviceMAC is %s\n", deviceMAC));
    CcspTraceInfo((" Source derived is %s\n", source));

    wrp_msg = (wrp_msg_t *)malloc(sizeof(wrp_msg_t));


    if(wrp_msg != NULL)
    {
        memset(wrp_msg, 0, sizeof(wrp_msg_t));
        wrp_msg->msg_type = WRP_MSG_TYPE__EVENT;
        wrp_msg->u.event.payload = (void *)payload;
        wrp_msg->u.event.payload_size = payload_len;
        wrp_msg->u.event.source = source;
        wrp_msg->u.event.dest = dest;
        wrp_msg->u.event.content_type= contentType;

        CcspTraceInfo((" wrp_msg->msg_type :%d\n",wrp_msg->msg_type));
        if(wrp_msg->u.event.payload!=NULL)
            CcspTraceInfo((" wrp_msg->u.event.payload :%s\n",(char *)(wrp_msg->u.event.payload)));
        CcspTraceInfo((" wrp_msg->u.event.payload_size :%d\n",wrp_msg->u.event.payload_size));
        if(wrp_msg->u.event.source!=NULL)
            CcspTraceInfo((" wrp_msg->u.event.source :%s\n",wrp_msg->u.event.source));
        if(wrp_msg->u.event.dest!=NULL)
            CcspTraceInfo((" wrp_msg->u.event.dest :%s\n",wrp_msg->u.event.dest));
        if(wrp_msg->u.event.content_type!=NULL)
            CcspTraceInfo((" wrp_msg->u.event.content_type :%s\n",wrp_msg->u.event.content_type));

        while(retry_count<=5)
        {
            backoffRetryTime = (int) pow(2, c) -1;

            CcspTraceInfo((" retry_count : %d\n",retry_count));
            CcspTraceInfo(("XDSL REPORT %s : LINE %d client_instance = %p\n", __FUNCTION__, __LINE__, client_instance));
            sendStatus = libparodus_send(client_instance, wrp_msg);
            CcspTraceInfo((" sendStatus is %d\n",sendStatus));
            if(sendStatus == 0)
            {
                retry_count = 0;
                CcspTraceInfo((" Sent message successfully to parodus\n"));
                break;
            }
            else
            {
                CcspTraceError((" Failed to send message: '%s', retrying ....\n",libparodus_strerror(sendStatus)));
                CcspTraceInfo((" backoffRetryTime %d seconds\n", backoffRetryTime));
                sleep(backoffRetryTime);
                c++;
                retry_count++;
            }
        }

        CcspTraceInfo((" Before freeing wrp_msg\n"));
        CcspTraceInfo(("XDSL REPORT %s : LINE %d free()\n", __FUNCTION__,  __LINE__));
        free(wrp_msg);
        CcspTraceInfo((" After freeing wrp_msg\n"));
    }

    CcspTraceInfo(("  <======== End of sendWebpaMsg =======>\n"));

    CcspTraceInfo((" XDSL REPORT %s EXIT\n", __FUNCTION__ ));

}

const char *rdk_logger_module_fetch(void)
{
    return "LOG.RDK.XDSLMANAGER";
}

