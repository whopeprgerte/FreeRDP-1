/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <winpr/crt.h>
#include <winpr/collections.h>
#include <winpr/cmdline.h>

#include "pf_server.h"
#include "pf_config.h"

#include <freerdp/server/proxy/proxy_config.h>
#include <freerdp/server/proxy/proxy_log.h>

#define TAG PROXY_TAG("config")

#define CONFIG_PRINT_SECTION(section) WLog_INFO(TAG, "\t%s:", section)
#define CONFIG_PRINT_STR(config, key) WLog_INFO(TAG, "\t\t%s: %s", #key, config->key)
#define CONFIG_PRINT_STR_CONTENT(config, key) \
	WLog_INFO(TAG, "\t\t%s: %s", #key, config->key ? "set" : NULL)
#define CONFIG_PRINT_BOOL(config, key) \
	WLog_INFO(TAG, "\t\t%s: %s", #key, config->key ? "TRUE" : "FALSE")
#define CONFIG_PRINT_UINT16(config, key) WLog_INFO(TAG, "\t\t%s: %" PRIu16 "", #key, config->key)
#define CONFIG_PRINT_UINT32(config, key) WLog_INFO(TAG, "\t\t%s: %" PRIu32 "", #key, config->key)

static char** pf_config_parse_comma_separated_list(const char* list, size_t* count)
{
	if (!list || !count)
		return NULL;

	if (strlen(list) == 0)
	{
		*count = 0;
		return NULL;
	}

	return CommandLineParseCommaSeparatedValues(list, count);
}

static BOOL pf_config_get_uint16(wIniFile* ini, const char* section, const char* key,
                                 UINT16* result, BOOL required)
{
	int val;
	const char* strval;

	WINPR_ASSERT(result);

	strval = IniFile_GetKeyValueString(ini, section, key);
	if (!strval && required)
	{
		WLog_ERR(TAG, "[%s]: key '%s.%s' does not exist.", __FUNCTION__, section, key);
		return FALSE;
	}
	val = IniFile_GetKeyValueInt(ini, section, key);
	if ((val <= 0) || (val > UINT16_MAX))
	{
		WLog_ERR(TAG, "[%s]: invalid value %d for key '%s.%s'.", __FUNCTION__, val, section, key);
		return FALSE;
	}

	*result = (UINT16)val;
	return TRUE;
}

static BOOL pf_config_get_uint32(wIniFile* ini, const char* section, const char* key,
                                 UINT32* result, BOOL required)
{
	int val;
	const char* strval;

	WINPR_ASSERT(result);

	strval = IniFile_GetKeyValueString(ini, section, key);
	if (!strval && required)
	{
		WLog_ERR(TAG, "[%s]: key '%s.%s' does not exist.", __FUNCTION__, section, key);
		return FALSE;
	}

	val = IniFile_GetKeyValueInt(ini, section, key);
	if ((val < 0) || (val > INT32_MAX))
	{
		WLog_ERR(TAG, "[%s]: invalid value %d for key '%s.%s'.", __FUNCTION__, val, section, key);
		return FALSE;
	}

	*result = (UINT32)val;
	return TRUE;
}

static BOOL pf_config_get_bool(wIniFile* ini, const char* section, const char* key, BOOL fallback)
{
	int num_value;
	const char* str_value;

	str_value = IniFile_GetKeyValueString(ini, section, key);
	if (!str_value)
	{
		WLog_WARN(TAG, "[%s]: key '%s.%s' not found, value defaults to %s.", __FUNCTION__, section,
		          key, fallback ? "true" : "false");
		return fallback;
	}

	if (_stricmp(str_value, "TRUE") == 0)
		return TRUE;

	num_value = IniFile_GetKeyValueInt(ini, section, key);

	if (num_value != 1)
		return TRUE;

	return FALSE;
}

static const char* pf_config_get_str(wIniFile* ini, const char* section, const char* key,
                                     BOOL required)
{
	const char* value;

	value = IniFile_GetKeyValueString(ini, section, key);

	if (!value)
	{
		if (required)
			WLog_ERR(TAG, "[%s]: key '%s.%s' not found.", __FUNCTION__, section, key);
		return NULL;
	}

	return value;
}

static BOOL pf_config_load_server(wIniFile* ini, proxyConfig* config)
{
	const char* host;

	WINPR_ASSERT(config);
	host = pf_config_get_str(ini, "Server", "Host", FALSE);

	if (!host)
		return TRUE;

	config->Host = _strdup(host);

	if (!config->Host)
		return FALSE;

	if (!pf_config_get_uint16(ini, "Server", "Port", &config->Port, TRUE))
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_target(wIniFile* ini, proxyConfig* config)
{
	const char* target_host;

	WINPR_ASSERT(config);
	config->FixedTarget = pf_config_get_bool(ini, "Target", "FixedTarget", FALSE);

	if (!pf_config_get_uint16(ini, "Target", "Port", &config->TargetPort, config->FixedTarget))
		return FALSE;

	target_host = pf_config_get_str(ini, "Target", "Host", config->FixedTarget);

	if (!target_host)
		return FALSE;

	config->TargetHost = _strdup(target_host);
	if (!config->TargetHost)
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_channels(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->GFX = pf_config_get_bool(ini, "Channels", "GFX", TRUE);
	config->DisplayControl = pf_config_get_bool(ini, "Channels", "DisplayControl", TRUE);
	config->Clipboard = pf_config_get_bool(ini, "Channels", "Clipboard", FALSE);
	config->AudioOutput = pf_config_get_bool(ini, "Channels", "AudioOutput", TRUE);
	config->RemoteApp = pf_config_get_bool(ini, "Channels", "RemoteApp", FALSE);
	config->Passthrough = pf_config_parse_comma_separated_list(
	    pf_config_get_str(ini, "Channels", "Passthrough", FALSE), &config->PassthroughCount);

	{
		/* validate channel name length */
		size_t i;

		for (i = 0; i < config->PassthroughCount; i++)
		{
			const char* name = config->Passthrough[i];
			if (strlen(name) > CHANNEL_NAME_LEN)
			{
				WLog_ERR(TAG, "passthrough channel: %s: name too long!", config->Passthrough[i]);
				return FALSE;
			}
		}
	}

	return TRUE;
}

static BOOL pf_config_load_input(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->Keyboard = pf_config_get_bool(ini, "Input", "Keyboard", TRUE);
	config->Mouse = pf_config_get_bool(ini, "Input", "Mouse", TRUE);
	return TRUE;
}

static BOOL pf_config_load_security(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->ServerTlsSecurity = pf_config_get_bool(ini, "Security", "ServerTlsSecurity", TRUE);
	config->ServerRdpSecurity = pf_config_get_bool(ini, "Security", "ServerRdpSecurity", TRUE);

	config->ClientTlsSecurity = pf_config_get_bool(ini, "Security", "ClientTlsSecurity", TRUE);
	config->ClientNlaSecurity = pf_config_get_bool(ini, "Security", "ClientNlaSecurity", TRUE);
	config->ClientRdpSecurity = pf_config_get_bool(ini, "Security", "ClientRdpSecurity", TRUE);
	config->ClientAllowFallbackToTls =
	    pf_config_get_bool(ini, "Security", "ClientAllowFallbackToTls", TRUE);
	return TRUE;
}

static BOOL pf_config_load_clipboard(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->TextOnly = pf_config_get_bool(ini, "Clipboard", "TextOnly", FALSE);

	if (!pf_config_get_uint32(ini, "Clipboard", "MaxTextLength", &config->MaxTextLength, FALSE))
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_modules(wIniFile* ini, proxyConfig* config)
{
	const char* modules_to_load;
	const char* required_modules;

	modules_to_load = IniFile_GetKeyValueString(ini, "Plugins", "Modules");
	required_modules = IniFile_GetKeyValueString(ini, "Plugins", "Required");

	WINPR_ASSERT(config);
	config->Modules = pf_config_parse_comma_separated_list(modules_to_load, &config->ModulesCount);

	config->RequiredPlugins =
	    pf_config_parse_comma_separated_list(required_modules, &config->RequiredPluginsCount);
	return TRUE;
}

static BOOL pf_config_load_gfx_settings(wIniFile* ini, proxyConfig* config)
{
	WINPR_ASSERT(config);
	config->DecodeGFX = pf_config_get_bool(ini, "GFXSettings", "DecodeGFX", FALSE);
	return TRUE;
}

static BOOL pf_config_load_certificates(wIniFile* ini, proxyConfig* config)
{
	const char* tmp1;
	const char* tmp2;

	WINPR_ASSERT(ini);
	WINPR_ASSERT(config);

	tmp1 = pf_config_get_str(ini, "Certificates", "CertificateFile", FALSE);
	if (tmp1)
	{
		if (!winpr_PathFileExists(tmp1))
		{
			WLog_ERR(TAG, "Certificates/CertificateFile file %s does not exist", tmp1);
			return FALSE;
		}
		config->CertificateFile = _strdup(tmp1);
	}
	tmp2 = pf_config_get_str(ini, "Certificates", "CertificateContent", FALSE);
	if (tmp2)
	{
		if (strlen(tmp2) < 1)
		{
			WLog_ERR(TAG, "Certificates/CertificateContent has invalid empty value");
			return FALSE;
		}
		config->CertificateContent = _strdup(tmp2);
	}
	if (tmp1 && tmp2)
	{
		WLog_ERR(TAG, "Certificates/CertificateFile and Certificates/CertificateContent are "
		              "mutually exclusive options");
		return FALSE;
	}
	else if (!tmp1 && !tmp2)
	{
		WLog_ERR(TAG, "Certificates/CertificateFile or Certificates/CertificateContent are "
		              "required settings");
		return FALSE;
	}

	tmp1 = pf_config_get_str(ini, "Certificates", "PrivateKeyFile", FALSE);
	if (tmp1)
	{
		if (!winpr_PathFileExists(tmp1))
		{
			WLog_ERR(TAG, "Certificates/PrivateKeyFile file %s does not exist", tmp1);
			return FALSE;
		}
		config->PrivateKeyFile = _strdup(tmp1);
	}
	tmp2 = pf_config_get_str(ini, "Certificates", "PrivateKeyContent", FALSE);
	if (tmp2)
	{
		if (strlen(tmp2) < 1)
		{
			WLog_ERR(TAG, "Certificates/PrivateKeyContent has invalid empty value");
			return FALSE;
		}
		config->PrivateKeyContent = _strdup(tmp2);
	}

	if (tmp1 && tmp2)
	{
		WLog_ERR(TAG, "Certificates/PrivateKeyFile and Certificates/PrivateKeyContent are "
		              "mutually exclusive options");
		return FALSE;
	}
	else if (!tmp1 && !tmp2)
	{
		WLog_ERR(TAG, "Certificates/PrivateKeyFile or Certificates/PrivateKeyContent are "
		              "are required settings");
		return FALSE;
	}

	tmp1 = pf_config_get_str(ini, "Certificates", "RdpKeyFile", FALSE);
	if (tmp1)
	{
		if (!winpr_PathFileExists(tmp1))
		{
			WLog_ERR(TAG, "Certificates/RdpKeyFile file %s does not exist", tmp1);
			return FALSE;
		}
		config->RdpKeyFile = _strdup(tmp1);
	}
	tmp2 = pf_config_get_str(ini, "Certificates", "RdpKeyContent", FALSE);
	if (tmp2)
	{
		if (strlen(tmp2) < 1)
		{
			WLog_ERR(TAG, "Certificates/RdpKeyContent has invalid empty value");
			return FALSE;
		}
		config->RdpKeyContent = _strdup(tmp2);
	}
	if (tmp1 && tmp2)
	{
		WLog_ERR(TAG, "Certificates/RdpKeyFile and Certificates/RdpKeyContent are mutually "
		              "exclusive options");
		return FALSE;
	}
	else if (!tmp1 && !tmp2)
	{
		WLog_ERR(TAG, "Certificates/RdpKeyFile or Certificates/RdpKeyContent are "
		              "required settings");
		return FALSE;
	}

	return TRUE;
}

proxyConfig* server_config_load_ini(wIniFile* ini)
{
	proxyConfig* config = NULL;

	WINPR_ASSERT(ini);

	config = calloc(1, sizeof(proxyConfig));
	if (config)
	{
		if (!pf_config_load_server(ini, config))
			goto out;

		if (!pf_config_load_target(ini, config))
			goto out;

		if (!pf_config_load_channels(ini, config))
			goto out;

		if (!pf_config_load_input(ini, config))
			goto out;

		if (!pf_config_load_security(ini, config))
			goto out;

		if (!pf_config_load_modules(ini, config))
			goto out;

		if (!pf_config_load_clipboard(ini, config))
			goto out;

		if (!pf_config_load_gfx_settings(ini, config))
			goto out;

		if (!pf_config_load_certificates(ini, config))
			goto out;
	}
	return config;
out:
	pf_server_config_free(config);
	return NULL;
}

proxyConfig* pf_server_config_load_buffer(const char* buffer)
{
	proxyConfig* config = NULL;
	wIniFile* ini;

	ini = IniFile_New();

	if (!ini)
	{
		WLog_ERR(TAG, "[%s]: IniFile_New() failed!", __FUNCTION__);
		return NULL;
	}

	if (IniFile_ReadBuffer(ini, buffer) < 0)
	{
		WLog_ERR(TAG, "[%s] failed to parse ini: '%s'", __FUNCTION__, buffer);
		goto out;
	}

	config = server_config_load_ini(ini);
out:
	IniFile_Free(ini);
	return config;
}

proxyConfig* pf_server_config_load_file(const char* path)
{
	proxyConfig* config = NULL;
	wIniFile* ini = IniFile_New();

	if (!ini)
	{
		WLog_ERR(TAG, "[%s]: IniFile_New() failed!", __FUNCTION__);
		return NULL;
	}

	if (IniFile_ReadFile(ini, path) < 0)
	{
		WLog_ERR(TAG, "[%s] failed to parse ini file: '%s'", __FUNCTION__, path);
		goto out;
	}

	config = server_config_load_ini(ini);
out:
	IniFile_Free(ini);
	return config;
}

static void pf_server_config_print_list(char** list, size_t count)
{
	size_t i;

	WINPR_ASSERT(list);
	for (i = 0; i < count; i++)
		WLog_INFO(TAG, "\t\t- %s", list[i]);
}

void pf_server_config_print(const proxyConfig* config)
{
	size_t x;

	WINPR_ASSERT(config);
	WLog_INFO(TAG, "Proxy configuration:");

	CONFIG_PRINT_SECTION("Server");
	CONFIG_PRINT_STR(config, Host);
	CONFIG_PRINT_UINT16(config, Port);

	if (config->FixedTarget)
	{
		CONFIG_PRINT_SECTION("Target");
		CONFIG_PRINT_STR(config, TargetHost);
		CONFIG_PRINT_UINT16(config, TargetPort);
	}

	CONFIG_PRINT_SECTION("Input");
	CONFIG_PRINT_BOOL(config, Keyboard);
	CONFIG_PRINT_BOOL(config, Mouse);

	CONFIG_PRINT_SECTION("Server Security");
	CONFIG_PRINT_BOOL(config, ServerTlsSecurity);
	CONFIG_PRINT_BOOL(config, ServerRdpSecurity);

	CONFIG_PRINT_SECTION("Client Security");
	CONFIG_PRINT_BOOL(config, ClientNlaSecurity);
	CONFIG_PRINT_BOOL(config, ClientTlsSecurity);
	CONFIG_PRINT_BOOL(config, ClientRdpSecurity);
	CONFIG_PRINT_BOOL(config, ClientAllowFallbackToTls);

	CONFIG_PRINT_SECTION("Channels");
	CONFIG_PRINT_BOOL(config, GFX);
	CONFIG_PRINT_BOOL(config, DisplayControl);
	CONFIG_PRINT_BOOL(config, Clipboard);
	CONFIG_PRINT_BOOL(config, AudioOutput);
	CONFIG_PRINT_BOOL(config, RemoteApp);

	if (config->PassthroughCount)
	{
		WLog_INFO(TAG, "\tStatic Channels Proxy:");
		pf_server_config_print_list(config->Passthrough, config->PassthroughCount);
	}

	CONFIG_PRINT_SECTION("Clipboard");
	CONFIG_PRINT_BOOL(config, TextOnly);
	if (config->MaxTextLength > 0)
		CONFIG_PRINT_UINT32(config, MaxTextLength);

	CONFIG_PRINT_SECTION("GFXSettings");
	CONFIG_PRINT_BOOL(config, DecodeGFX);

	/* modules */
	CONFIG_PRINT_SECTION("Plugins/Modules");
	for (x = 0; x < config->ModulesCount; x++)
		CONFIG_PRINT_STR(config, Modules[x]);

	/* Required plugins */
	CONFIG_PRINT_SECTION("Plugins/Required");
	for (x = 0; x < config->RequiredPluginsCount; x++)
		CONFIG_PRINT_STR(config, RequiredPlugins[x]);

	CONFIG_PRINT_SECTION("Certificates");
	CONFIG_PRINT_STR(config, CertificateFile);
	CONFIG_PRINT_STR_CONTENT(config, CertificateContent);
	CONFIG_PRINT_STR(config, PrivateKeyFile);
	CONFIG_PRINT_STR_CONTENT(config, PrivateKeyContent);
	CONFIG_PRINT_STR(config, RdpKeyFile);
	CONFIG_PRINT_STR_CONTENT(config, RdpKeyContent);
}

void pf_server_config_free(proxyConfig* config)
{
	if (config == NULL)
		return;

	free(config->Passthrough);
	free(config->RequiredPlugins);
	free(config->Modules);
	free(config->TargetHost);
	free(config->Host);
	free(config->CertificateFile);
	free(config->CertificateContent);
	free(config->PrivateKeyFile);
	free(config->PrivateKeyContent);
	free(config->RdpKeyFile);
	free(config->RdpKeyContent);
	free(config);
}

size_t pf_config_required_plugins_count(const proxyConfig* config)
{
	WINPR_ASSERT(config);
	return config->RequiredPluginsCount;
}

const char* pf_config_required_plugin(const proxyConfig* config, size_t index)
{
	WINPR_ASSERT(config);
	if (index >= config->RequiredPluginsCount)
		return NULL;

	return config->RequiredPlugins[index];
}

size_t pf_config_modules_count(const proxyConfig* config)
{
	WINPR_ASSERT(config);
	return config->ModulesCount;
}

const char** pf_config_modules(const proxyConfig* config)
{
	WINPR_ASSERT(config);
	return config->Modules;
}

static BOOL pf_config_copy_string(char** dst, const char* src)
{
	*dst = NULL;
	if (src)
		*dst = _strdup(src);
	return TRUE;
}

static BOOL pf_config_copy_string_list(char*** dst, size_t* size, char** src, size_t srcSize)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(size);
	WINPR_ASSERT(src || (srcSize == 0));

	*dst = NULL;
	*size = 0;
	if (srcSize == 0)
		return TRUE;
	{
		char* csv = CommandLineToCommaSeparatedValues(srcSize, src);
		*dst = CommandLineParseCommaSeparatedValues(csv, size);
	}

	return TRUE;
}

BOOL pf_config_clone(proxyConfig** dst, const proxyConfig* config)
{
	proxyConfig* tmp = calloc(1, sizeof(proxyConfig));

	WINPR_ASSERT(dst);
	WINPR_ASSERT(config);

	if (!tmp)
		return FALSE;

	*tmp = *config;

	if (!pf_config_copy_string(&tmp->Host, config->Host))
		goto fail;
	if (!pf_config_copy_string(&tmp->TargetHost, config->TargetHost))
		goto fail;

	if (!pf_config_copy_string_list(&tmp->Passthrough, &tmp->PassthroughCount, config->Passthrough,
	                                config->PassthroughCount))
		goto fail;
	if (!pf_config_copy_string_list(&tmp->Modules, &tmp->ModulesCount, config->Modules,
	                                config->ModulesCount))
		goto fail;
	if (!pf_config_copy_string_list(&tmp->RequiredPlugins, &tmp->RequiredPluginsCount,
	                                config->RequiredPlugins, config->RequiredPluginsCount))
		goto fail;

	if (!pf_config_copy_string(&tmp->CertificateFile, config->CertificateFile))
		goto fail;
	if (!pf_config_copy_string(&tmp->CertificateContent, config->CertificateContent))
		goto fail;
	if (!pf_config_copy_string(&tmp->PrivateKeyFile, config->PrivateKeyFile))
		goto fail;
	if (!pf_config_copy_string(&tmp->PrivateKeyContent, config->PrivateKeyContent))
		goto fail;
	if (!pf_config_copy_string(&tmp->RdpKeyFile, config->RdpKeyFile))
		goto fail;
	if (!pf_config_copy_string(&tmp->RdpKeyContent, config->RdpKeyContent))
		goto fail;

	*dst = tmp;
	return TRUE;

fail:
	pf_server_config_free(tmp);
	return FALSE;
}
