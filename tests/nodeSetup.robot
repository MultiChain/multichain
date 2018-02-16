*** Settings ***
Library    Collections
Library    RequestsLibrary
Library    customAuthenticator.py

Suite Teardown  Delete All Sessions


*** Variables ***
# Default system address. Override when tested agains other instances.
&{headers}     Content-Type=application/json   Authorization=Basic cmtycGM6MlRSSkxXYWZNMXJES2hMRFpHV2I1c2JXNmNoTk1peVpOcDdBSk03YnRGNjM=
${base_url}    http://35.171.226.226:7194


*** Test Cases ***

Check node balance
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getinfo
    ${resp}=  Post Request  reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    Should Be True     ${resp.json()['result']['balance']}>0


Generate New Address
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getnewaddress
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0
    Set Suite Variable  ${dummy_user}   ${resp.json()['result']}


Get Addresses
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getaddresses
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0


Check Transaction Info
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${param_list}=      Create List    ${dummy_user}
    ${tuple} =    Evaluate    (10)
    Append to List     ${param_list}   ${tuple}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=send    params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200

    ${txid}=    Set Variable	${resp.json()['result']}

    ${param_list}=      Create List    ${txid}
    &{tx_data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getwallettransaction    params=${param_list}
    ${tx_resp}=    Post Request    reckordskeeper  /  data=${tx_data}  headers=${headers}
    Should Be Equal As Strings  ${tx_resp.status_code}  200
    Should Not Be Empty     ${resp.json()['result']}>0


Get Blockchain Info
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getblockchaininfo
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0
    Should Be Equal As Strings  ${resp.json()['result']['chainname']}  recordskeeper-test
    Should Be Equal As Strings  ${resp.json()['result']['chain']}  main


Grant permission to mine
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${param_list}=  Create List     mine    ${dummy_user}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=listpermissions  params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    Should be Empty     ${resp.json()['result']}

    ${param_list}=  Create List     ${dummy_user}     mine
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=grant     params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200

    ${txid}=    Create List     ${resp.json()['result']}

    ${tx_data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getwallettransaction    params=${txid}
    ${tx_resp}=    Post Request    reckordskeeper  /  data=${tx_data}  headers=${headers}
    Should Be Equal As Strings  ${tx_resp.status_code}  200
    Should be True     ${tx_resp.json()['result']['permissions'][0]['mine']}


Revoke permission to mine
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${param_list}=  Create List     mine    ${dummy_user}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=listpermissions  params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    Should not be Empty     ${resp.json()['result']}

    ${param_list}=  Create List     ${dummy_user}     mine
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=revoke     params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200

    Create Session  reckordskeeper  ${base_url}
    ${param_list}=  Create List     mine    ${dummy_user}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=listpermissions  params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    Should be Empty     ${resp.json()['result']}



Generate Key Pairs
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=createkeypairs
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0


Check Address Balance
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${param_list}=  Create List     ${dummy_user}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getaddressbalances  params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    Should be True     ${resp.json()['result'][0]['qty']}==0

    ${send_param_list}=  Create List     ${dummy_user}
    ${balance} =    Evaluate    (100)
    Append to List     ${send_param_list}   ${balance}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=send  params=${send_param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200

    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=getaddressbalances  params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    Should be True     ${resp.json()['result'][0]['qty']}==100


Publishing to Stream should not be allowed
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=publish
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  500
    Should Be Equal As Strings  ${resp.json()['error']['code']}  -702


Checking transactions linked to an account
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${param_list}=  Create List     ${dummy_user}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=listaddresstransactions     params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0


Checking transactions linked to a wallet
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${count} =    Evaluate    (10)
    ${param_list}=      Create List    ${count}
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=listwallettransactions     params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0

List Stream's Data
    [Tags]  post
    Create Session  reckordskeeper  ${base_url}
    ${param_list}=      Create List    root
    &{data}=  Create Dictionary  jsonrpc=1.0   id=curltext   method=liststreams     params=${param_list}
    ${resp}=    Post Request    reckordskeeper  /  data=${data}  headers=${headers}
    Should Be Equal As Strings  ${resp.status_code}  200
    ${addLength}=   Get Length  ${resp.json()['result']}
    Should be True     ${addLength}>0