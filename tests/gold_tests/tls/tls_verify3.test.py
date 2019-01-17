'''
'''
#  Licensed to the Apache Software Foundation (ASF) under one
#  or more contributor license agreements.  See the NOTICE file
#  distributed with this work for additional information
#  regarding copyright ownership.  The ASF licenses this file
#  to you under the Apache License, Version 2.0 (the
#  "License"); you may not use this file except in compliance
#  with the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

import os
Test.Summary = '''
Test tls server certificate verification options
'''

# need Curl
Test.SkipUnless(
    Condition.HasProgram("curl", "Curl need to be installed on system for this test to work")
)

# Define default ATS
ts = Test.MakeATSProcess("ts", select_ports=False)
server_foo = Test.MakeOriginServer("server_foo", ssl=True, options = {"--key": "{0}/signed-foo.key".format(Test.RunDirectory), "--cert": "{0}/signed-foo.pem".format(Test.RunDirectory)})
server_bar = Test.MakeOriginServer("server_bar", ssl=True, options = {"--key": "{0}/signed-bar.key".format(Test.RunDirectory), "--cert": "{0}/signed-bar.pem".format(Test.RunDirectory)})
server = Test.MakeOriginServer("server", ssl=True)

request_foo_header = {"headers": "GET / HTTP/1.1\r\nHost: foo.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
request_bad_foo_header = {"headers": "GET / HTTP/1.1\r\nHost: badfoo.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
request_bar_header = {"headers": "GET / HTTP/1.1\r\nHost: bar.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
request_bad_bar_header = {"headers": "GET / HTTP/1.1\r\nHost: badbar.com\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
response_header = {"headers": "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n", "timestamp": "1469733493.993", "body": ""}
server_foo.addResponse("sessionlog.json", request_foo_header, response_header)
server_foo.addResponse("sessionlog.json", request_bad_foo_header, response_header)
server_bar.addResponse("sessionlog.json", request_bar_header, response_header)
server_bar.addResponse("sessionlog.json", request_bad_bar_header, response_header)

# add ssl materials like key, certificates for the server
ts.addSSLfile("ssl/signed-foo.pem")
ts.addSSLfile("ssl/signed-foo.key")
ts.addSSLfile("ssl/signed-bar.pem")
ts.addSSLfile("ssl/signed-bar.key")
ts.addSSLfile("ssl/server.pem")
ts.addSSLfile("ssl/server.key")
ts.addSSLfile("ssl/signer.pem")
ts.addSSLfile("ssl/signer.key")

ts.Variables.ssl_port = 4443
ts.Disk.remap_config.AddLine(
    'map https://foo.com:{1}/ https://127.0.0.1:{0}'.format(server_foo.Variables.Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map https://bob.foo.com:{1}/ https://127.0.0.1:{0}'.format(server_foo.Variables.Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map https://bar.com:{1}/ https://127.0.0.1:{0}'.format(server_bar.Variables.Port, ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map https://bob.bar.com:{1}/ https://127.0.0.1:{0}'.format(server_bar.Variables.Port,ts.Variables.ssl_port))
ts.Disk.remap_config.AddLine(
    'map / https://127.0.0.1:{0}'.format(server.Variables.Port))

ts.Disk.ssl_multicert_config.AddLine(
    'dest_ip=* ssl_cert_name=server.pem ssl_key_name=server.key'
)

# Case 1, global config policy=permissive properties=signature
#         override for foo.com policy=enforced properties=all
ts.Disk.records_config.update({
    'proxy.config.diags.debug.enabled': 1,
    'proxy.config.diags.debug.tags': 'ssl|http|url',
    'proxy.config.ssl.server.cert.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.server.private_key.path': '{0}'.format(ts.Variables.SSLDir),
    # enable ssl port
    'proxy.config.http.server_ports': '{0} {1}:proto=http2;http:ssl'.format(ts.Variables.port, ts.Variables.ssl_port),
    'proxy.config.ssl.server.cipher_suite': 'ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-SHA256:ECDHE-RSA-AES256-SHA384:AES128-GCM-SHA256:AES256-GCM-SHA384:ECDHE-RSA-RC4-SHA:ECDHE-RSA-AES128-SHA:ECDHE-RSA-AES256-SHA:RC4-SHA:RC4-MD5:AES128-SHA:AES256-SHA:DES-CBC3-SHA!SRP:!DSS:!PSK:!aNULL:!eNULL:!SSLv2',
    # set global policy
    'proxy.config.ssl.client.verify.server.policy': 'PERMISSIVE',
    'proxy.config.ssl.client.verify.server.properties': 'ALL',
    'proxy.config.ssl.client.CA.cert.path': '{0}'.format(ts.Variables.SSLDir),
    'proxy.config.ssl.client.CA.cert.filename': 'signer.pem',
    'proxy.config.url_remap.pristine_host_hdr': 1
})

ts.Disk.ssl_server_name_yaml.AddLines([
  '- fqdn: bob.bar.com',
  '  verify_server_policy: ENFORCED',
  '  verify_server_properties: ALL',
  '- fqdn: bob.*.com',
  '  verify_server_policy: ENFORCED',
  '  verify_server_properties: SIGNATURE',
  "- fqdn: '*bar.com'",
  '  verify_server_policy: DISABLED',
])

tr = Test.AddTestRun("foo.com Permissive-Test")
tr.Setup.Copy("ssl/signed-foo.key")
tr.Setup.Copy("ssl/signed-foo.pem")
tr.Setup.Copy("ssl/signed-bar.key")
tr.Setup.Copy("ssl/signed-bar.pem")
tr.Processes.Default.Command = "curl -v -k --resolve 'foo.com:{0}:127.0.0.1' https://foo.com:{0}".format(ts.Variables.ssl_port)
tr.ReturnCode = 0
tr.Processes.Default.StartBefore(server_foo)
tr.Processes.Default.StartBefore(server_bar)
tr.Processes.Default.StartBefore(server)
tr.Processes.Default.StartBefore(Test.Processes.ts, ready=When.PortOpen(ts.Variables.ssl_port))
tr.StillRunningAfter = server
tr.StillRunningAfter = ts
tr.Processes.Default.TimeOut = 5
tr.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should have succeeded")
tr.TimeOut = 5

tr2 = Test.AddTestRun("bob.bar.com Override-enforcing-Test")
tr2.Processes.Default.Command = "curl -v -k --resolve 'bob.bar.com:{0}:127.0.0.1' https://bob.bar.com:{0}/".format(ts.Variables.ssl_port)
tr2.ReturnCode = 0
tr2.StillRunningAfter = server
tr2.Processes.Default.TimeOut = 5
tr2.StillRunningAfter = ts
tr2.Processes.Default.Streams.stdout = Testers.ContainsExpression("Could Not Connect", "Curl attempt should have succeeded")
tr2.TimeOut = 5

tr3 = Test.AddTestRun("bob.foo.com override-enforcing-name-test")
tr3.Processes.Default.Command = "curl -v -k --resolve 'bob.foo.com:{0}:127.0.0.1' https://bob.foo.com:{0}/".format(ts.Variables.ssl_port)
tr3.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should not fail")
tr3.ReturnCode = 0
tr3.StillRunningAfter = server
tr3.StillRunningAfter = ts
tr3.Processes.Default.TimeOut = 5
tr3.TimeOut = 5

tr3 = Test.AddTestRun("random.bar.com override-no-test")
tr3.Processes.Default.Command = "curl -v -k --resolve 'random.bar.com:{0}:127.0.0.1' https://random.bar.com:{0}".format(ts.Variables.ssl_port)
tr3.Processes.Default.Streams.stdout = Testers.ExcludesExpression("Could Not Connect", "Curl attempt should not fail")
tr3.ReturnCode = 0
tr3.StillRunningAfter = server
tr3.StillRunningAfter = ts
tr3.Processes.Default.TimeOut = 5
tr3.TimeOut = 5


# Over riding the built in ERROR check since we expect tr3 to fail
ts.Disk.diags_log.Content = Testers.ExcludesExpression("verification failed", "Make sure the signatures didn't fail")
ts.Disk.diags_log.Content += Testers.ContainsExpression("WARNING: SNI \(bob.bar.com\) not in certificate", "Make sure bob.bar name checked failed.")