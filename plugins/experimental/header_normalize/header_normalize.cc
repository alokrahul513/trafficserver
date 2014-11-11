//////////////////////////////////////////////////////////////////////////////////////////////
// header_normalize::
//
// ATS plugin to convert headers into camel-case. It may be useful to solve
// interworking issues with legacy origins not supporting lower case headers
// required by protocols such as spdy/http2 etc.
//
// To use this plugin, configure a remap.config rule like
//
//   map http://foo.com http://bar.com @plugin=header_normalize.so
//
//
// The list of of all available parameters is in the README.
//
//
// Note that the path to the plugin itself must be absolute, and by default it is
//
//    /home/y/libexec/trafficserver/header_normalize.so
//
//

#define UNUSED __attribute__ ((unused))
static char UNUSED rcsId__header_normalize_cc[] = "@(#) $Id: header_normalize.cc 218 2014-11-11 01:29:16Z sudheerv $ built on " __DATE__ " " __TIME__;


#include <sys/time.h>
#include <ts/ts.h>
#include <ts/remap.h>
#include <set>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <netdb.h>
#include <map>

using namespace std;
///////////////////////////////////////////////////////////////////////////////
// Some constants.
//
const char* PLUGIN_NAME = "header_normalize";

std::map<std::string, std::string, std::less<std::string> >  hdrMap;

static void
buildHdrMap()
{
  hdrMap["accept"] = "Accept";
  hdrMap["accept-charset"] = "Accept-Charset";
  hdrMap["accept-encoding"] = "Accept-Encoding";
  hdrMap["accept-language"] = "Accept-Language";
  hdrMap["accept-ranges"] = "Accept-Ranges";
  hdrMap["age"] = "Age";
  hdrMap["allow"] = "Allow";
  hdrMap["approved"] = "Approved";
  hdrMap["bytes"] = "Bytes";
  hdrMap["cache-control"] = "Cache-Control";
  hdrMap["client-ip"] = "Client-Ip";
  hdrMap["connection"] = "Connection";
  hdrMap["content-base"] = "Content-Base";
  hdrMap["content-encoding"] = "Content-Encoding";
  hdrMap["content-language"] = "Content-Language";
  hdrMap["content-length"] = "Content-Length";
  hdrMap["content-location"] = "Content-Location";
  hdrMap["content-md5"] = "Content-MD5";
  hdrMap["content-range"] = "Content-Range";
  hdrMap["content-type"] = "Content-Type";
  hdrMap["control"] = "Control";
  hdrMap["cookie"] = "Cookie";
  hdrMap["date"] = "Date";
  hdrMap["distribution"] = "Distribution";
  hdrMap["etag"] = "Etag";
  hdrMap["expect"] = "Expect";
  hdrMap["expires"] = "Expires";
  hdrMap["followup-to"] = "Followup-To";
  hdrMap["from"] = "From";
  hdrMap["host"] = "Host";
  hdrMap["if-match"] = "If-Match";
  hdrMap["if-modified-since"] = "If-Modified-Since";
  hdrMap["if-none-match"] = "If-None-Match";
  hdrMap["if-range"] = "If-Range";
  hdrMap["if-unmodified-since"] = "If-Unmodified-Since";
  hdrMap["keep-alive"] = "Keep-Alive";
  hdrMap["keywords"] = "Keywords";
  hdrMap["last-modified"] = "Last-Modified";
  hdrMap["lines"] = "Lines";
  hdrMap["location"] = "Location";
  hdrMap["max-forwards"] = "Max-Forwards";
  hdrMap["message-id"] = "Message-Id";
  hdrMap["newsgroups"] = "Newsgroups";
  hdrMap["organization"] = "Organization";
  hdrMap["path"] = "Path";
  hdrMap["pragma"] = "Pragma";
  hdrMap["proxy-authenticate"] = "Proxy-Authenticate";
  hdrMap["proxy-authorization"] = "Proxy-Authorization";
  hdrMap["proxy-connection"] = "Proxy-Connection";
  hdrMap["public"] = "Public";
  hdrMap["range"] = "Range";
  hdrMap["references"] = "References";
  hdrMap["referer"] = "Referer";
  hdrMap["reply-to"] = "Reply-To";
  hdrMap["retry-after"] = "Retry-After";
  hdrMap["sender"] = "Sender";
  hdrMap["server"] = "Server";
  hdrMap["set-cookie"] = "Set-Cookie";
  hdrMap["strict-transport-security"] = "Strict-Transport-Security";
  hdrMap["subject"] = "Subject";
  hdrMap["summary"] = "Summary";
  hdrMap["te"] = "Te";
  hdrMap["transfer-encoding"] = "Transfer-Encoding";
  hdrMap["upgrade"] = "Upgrade";
  hdrMap["user-agent"] = "User-Agent";
  hdrMap["vary"] = "Vary";
  hdrMap["via"] = "Via";
  hdrMap["warning"] = "Warning";
  hdrMap["www-authenticate"] = "Www-Authenticate";
  hdrMap["xref"] = "Xref";
  hdrMap["@datainfo"] = "@DataInfo";
  hdrMap["x-id"] = "X-ID";
  hdrMap["x-forwarded-for"] = "X-Forwarded-For";
  hdrMap["sec-websocket-key"] = "Sec-WebSocket-Key";
  hdrMap["sec-websocket-version"] = "Sec-WebSocket-Version";
}

///////////////////////////////////////////////////////////////////////////////
// Initialize the plugin.
//
//
//
TSReturnCode
TSRemapInit(TSRemapInterface *api_info, char *errbuf, int errbuf_size)
{
  if (!api_info) {
    strncpy(errbuf, "[tsremap_init] - Invalid TSREMAP_INTERFACE argument", errbuf_size - 1);
    return TS_ERROR;
  }
  if (api_info->tsremap_version < TSREMAP_VERSION) {
    snprintf(errbuf, errbuf_size - 1, "[tsremap_init] - Incorrect API version %ld.%ld",
             api_info->tsremap_version >> 16, (api_info->tsremap_version & 0xffff));
    return TS_ERROR;
  }
  buildHdrMap();
  TSDebug(PLUGIN_NAME, "plugin is succesfully initialized");
  return TS_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
// One instance per remap.config invocation.
//
TSReturnCode
TSRemapNewInstance(int /* argc */, char * /* argv[] */, void ** /* ih */, char * /* errbuf */, int /* errbuf_size */)
{
  return TS_SUCCESS;
}

void
TSRemapDelteInstance(void * /* ih */)
{
}

static int
read_request_hook(TSCont /* contp */, TSEvent /* event */, void *edata)
{

  TSHttpTxn rh = (TSHttpTxn) edata;

  TSMLoc hdr, next_hdr;
  TSMBuffer hdr_bufp;
  TSMLoc req_hdrs;

  if (TSHttpTxnClientReqGet(rh, &hdr_bufp, &req_hdrs) == TS_SUCCESS) {
    hdr = TSMimeHdrFieldGet(hdr_bufp, req_hdrs, 0);
    int n_mime_headers = TSMimeHdrFieldsCount(hdr_bufp, req_hdrs);

    TSDebug(PLUGIN_NAME, "*** Camel Casing %u hdrs in the request", n_mime_headers);

    for (int i = 0; i < n_mime_headers; ++i) {
      if (hdr == NULL)
	break;
      next_hdr = TSMimeHdrFieldNext(hdr_bufp, req_hdrs, hdr);
      int old_hdr_len;
      const char* old_hdr_name = TSMimeHdrFieldNameGet(hdr_bufp, req_hdrs, hdr, &old_hdr_len);

      // TSMimeHdrFieldNameGet returns the MIME_FIELD_NAME
      // for all MIME hdrs, which is always in Camel Case
      if (islower(old_hdr_name[0])) {
         TSDebug(PLUGIN_NAME, "*** non MIME Hdr %s, leaving it for now", old_hdr_name);

         TSHandleMLocRelease(hdr_bufp, req_hdrs, hdr);
         hdr = next_hdr;
	 continue;
      }

      int hdr_value_len = 0;
      const char *hdr_value = TSMimeHdrFieldValueStringGet(hdr_bufp, req_hdrs, hdr, 0, &hdr_value_len);

      // hdr returned by TSMimeHdrFieldNameGet is already
      // in camel case, just destroy the lowercase spdy header
      // and replace it with TSMimeHdrFieldNameGet
      char* new_hdr_name = (char*)old_hdr_name;
      if (new_hdr_name) {
        TSMLoc new_hdr_loc;
        TSReturnCode rval = TSMimeHdrFieldCreateNamed(hdr_bufp, req_hdrs, new_hdr_name, old_hdr_len, &new_hdr_loc);

        if (rval == TS_SUCCESS) {
          TSDebug(PLUGIN_NAME, "*** hdr convert %s to %s", old_hdr_name, new_hdr_name);
          TSMimeHdrFieldValueStringSet(hdr_bufp, req_hdrs, new_hdr_loc, -1, hdr_value, hdr_value_len);
          TSMimeHdrFieldAppend(hdr_bufp, req_hdrs, new_hdr_loc);
          TSHandleMLocRelease(hdr_bufp, req_hdrs, new_hdr_loc);
        }

        TSMimeHdrFieldDestroy(hdr_bufp, req_hdrs, hdr);
      } else {
          TSDebug(PLUGIN_NAME, "*** can't find hdr %s in hdrMap", old_hdr_name);
      }

      TSHandleMLocRelease(hdr_bufp, req_hdrs, hdr);
      hdr = next_hdr;
    }

    TSHandleMLocRelease(hdr_bufp, TS_NULL_MLOC, req_hdrs);
  }

  TSHttpTxnReenable(rh, TS_EVENT_HTTP_CONTINUE);

  return 0;
}

void
TSPluginInit(int /* argc */, const char * /* argv[] */)
{
  TSDebug(PLUGIN_NAME, "initializing plugin");
  TSCont contp;
  buildHdrMap();
  contp = TSContCreate(read_request_hook, NULL);
  TSHttpHookAdd(TS_HTTP_READ_REQUEST_HDR_HOOK, contp);
}

///////////////////////////////////////////////////////////////////////////////
// This is the main "entry" point for the plugin, called for every request.
//
TSRemapStatus
TSRemapDoRemap (void * /* ih */, TSHttpTxn rh, TSRemapRequestInfo * /* rri */)
{
  read_request_hook(NULL, TS_EVENT_HTTP_READ_REQUEST_HDR, rh);
  return TSREMAP_DID_REMAP;
}
