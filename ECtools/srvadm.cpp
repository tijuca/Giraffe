/*
 * Copyright 2005-2016 Zarafa and its licensors
 * Copyright 2018, Kopano and its licensors
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <cstdlib>
#include <popt.h>
#include <mapidefs.h>
#include <kopano/CommonUtil.h>
#include <kopano/ECConfig.h>
#include <kopano/ECLogger.h>
#include <kopano/IECInterfaces.hpp>
#include <kopano/MAPIErrors.h>
#include <kopano/stringutil.h>

using namespace KC;
static int opt_purge_deferred, opt_purge_softdelete;
static unsigned int opt_cache_bits;
static const char *opt_config_file, *opt_host, *opt_clear_cache;
static std::unique_ptr<ECConfig> adm_config;

static constexpr const struct poptOption adm_options[] = {
	{"clear-cache", 0, POPT_ARG_STRING, &opt_clear_cache, 0, "Clear one or more caches"},
	{"purge-deferred", 0, POPT_ARG_NONE, &opt_purge_deferred, 0, "Purge all items in the deferred update table"},
	{"purge-softdelete", 0, POPT_ARG_INT, &opt_purge_softdelete, 0, "Purge softdeleted items older than N days"},
	{nullptr, 'c', POPT_ARG_STRING, &opt_config_file, 'c', "Specify alternate config file"},
	{nullptr, 'h', POPT_ARG_STRING, &opt_host, 0, "URI for server"},
	POPT_AUTOHELP
	{nullptr},
};

static constexpr const configsetting_t adm_config_defaults[] = {
	{"server_socket", "default:"},
	{"sslkey_file", ""},
	{"sslkey_pass", ""},
	{nullptr},
};

static HRESULT adm_clear_cache(IECServiceAdmin *svcadm)
{
	auto ret = svcadm->PurgeCache(opt_cache_bits);
	if (ret != hrSuccess)
		kc_perror("Clearing caches failed", ret);
	printf("Caches have been cleared.\n");
	return hrSuccess;
}

static HRESULT adm_purge_deferred(IECServiceAdmin *svcadm)
{
	using namespace std::chrono;
	auto last = steady_clock::now() - seconds(1);
	while (true) {
		ULONG rem;
		auto ret = svcadm->PurgeDeferredUpdates(&rem);
		if (ret == MAPI_E_NOT_FOUND)
			break;
		else if (ret != hrSuccess)
			return kc_perror("Purge failed", ret);
		auto now = decltype(last)::clock::now();
		if (last + seconds(1) > now)
			continue;
		last = now;
		if (isatty(STDERR_FILENO))
			fprintf(stderr, "\r\e[2K""Remaining deferred records: %u", rem); // ]
		else
			fprintf(stderr, "Remaining deferred records: %u\n", rem);
		fflush(stderr);
	}
	if (isatty(STDERR_FILENO))
		fprintf(stderr, "\r\e[2K"); // ]
	fprintf(stderr, "Deferred records processed.\n");
	return hrSuccess;
}

static HRESULT adm_purge_softdelete(IECServiceAdmin *svcadm)
{
	auto ret = svcadm->PurgeSoftDelete(opt_purge_softdelete);
	if (ret == MAPI_E_BUSY) {
		printf("Softdelete purge already running.\n");
		return hrSuccess;
	} else if (ret != hrSuccess) {
		return kc_perror("Softdelete purge failed", ret);
	}
	printf("Softdelete purge done.\n");
	return hrSuccess;
}

static HRESULT adm_perform()
{
	KServerContext srvctx;
	srvctx.m_app_misc = "srvadm";
	if (opt_host == nullptr)
		opt_host = GetServerUnixSocket(adm_config->GetSetting("server_socket"));
	srvctx.m_host = opt_host;
	auto ret = srvctx.logon();
	if (ret != hrSuccess)
		return kc_perror("KServerContext::logon", ret);
	if (opt_clear_cache)
		return adm_clear_cache(srvctx.m_svcadm);
	if (opt_purge_deferred)
		return adm_purge_deferred(srvctx.m_svcadm);
	if (opt_purge_softdelete)
		return adm_purge_softdelete(srvctx.m_svcadm);
	return MAPI_E_CALL_FAILED;
}

static unsigned int adm_parse_cache(const char *arglist)
{
	if (*arglist == '\0') {
		fprintf(stderr, "No caches were selected, nothing will be done.\n");
		return 0;
	}
	static const std::map<std::string, unsigned int> map = {
		{"all", PURGE_CACHE_ALL},
		{"quota", PURGE_CACHE_QUOTA},
		{"quotadefault", PURGE_CACHE_QUOTADEFAULT},
		{"object", PURGE_CACHE_OBJECTS},
		{"store", PURGE_CACHE_STORES},
		{"acl", PURGE_CACHE_ACL},
		{"cell", PURGE_CACHE_CELL},
		{"index1", PURGE_CACHE_INDEX1},
		{"index2", PURGE_CACHE_INDEX2},
		{"indexedproperty", PURGE_CACHE_INDEXEDPROPERTIES},
		{"userobject", PURGE_CACHE_USEROBJECT},
		{"externid", PURGE_CACHE_EXTERNID},
		{"userdetail", PURGE_CACHE_USERDETAILS},
		{"server", PURGE_CACHE_SERVER},
	};
	unsigned int bits = 0;
	for (const auto &arg : tokenize(arglist, ",")) {
		auto e = map.find(arg);
		if (e == map.cend()) {
			fprintf(stderr, "Unknown cache: \"%s\"\n", arg.c_str());
			return 0;
		}
		bits |= e->second;
	}
	return bits;
}

static bool adm_parse_options(int &argc, char **&argv)
{
	adm_config.reset(ECConfig::Create(adm_config_defaults));
	opt_config_file = ECConfig::GetDefaultPath("admin.cfg");
	auto ctx = poptGetContext(nullptr, argc, const_cast<const char **>(argv), adm_options, 0);
	int c;
	while ((c = poptGetNextOpt(ctx)) >= 0) {
		if (c == 'c') {
			adm_config->LoadSettings(opt_config_file);
			if (adm_config->HasErrors()) {
				fprintf(stderr, "Error reading config file %s\n", opt_config_file);
				return false;
			}
		}
	}
	if (c < -1) {
		fprintf(stderr, "%s\n", poptStrerror(c));
		poptPrintHelp(ctx, stderr, 0);
		return false;
	}
	if (opt_clear_cache != nullptr) {
		opt_cache_bits = adm_parse_cache(opt_clear_cache);
		if (opt_cache_bits == 0)
			return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	ec_log_get()->SetLoglevel(EC_LOGLEVEL_INFO);
	if (!adm_parse_options(argc, argv))
		return EXIT_FAILURE;
	return adm_perform() == hrSuccess ? EXIT_SUCCESS : EXIT_FAILURE;
}
