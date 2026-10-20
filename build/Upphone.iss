; Instalador do Upphone SIP (Inno Setup 6)
; Normalmente chamado pelo build.ps1:
;   ISCC.exe /DAppVersion=1.2.0 /DPlatform=x64 /DSourceDir=..\dist\1.2.0\x64 Upphone.iss
;
; Ativacao (docs/PROVISIONAMENTO.md): a pagina "Ativacao" pede o codigo da Upp (opcional) e o confere com o
; servidor antes de instalar, sem usa-lo. O codigo fica em %ProgramData%\Upphone SIP\ativacao.ini e quem
; ativa e o proprio app, ao abrir com o usuario final (as credenciais ficam protegidas no perfil dele).
; Parametros para instalacao silenciosa (por exemplo /VERYSILENT):
;   /CODIGO=XXXX-XXXX-XXXX-XXXX   codigo de ativacao
;   /SERVIDOR=https://...         servidor de provisionamento (obrigatorio para codigos de desenvolvimento "D")
;   /USUARIO=DOMINIO\usuario      so esse usuario do Windows usa o codigo

#ifndef AppVersion
  #define AppVersion "1.2.0"
#endif
#ifndef Platform
  #define Platform "x64"
#endif
#ifndef SourceDir
  #define SourceDir "..\dist\" + AppVersion + "\" + Platform
#endif

#define AppName "Upphone SIP"
#define AppExe "Upphone.exe"

[Setup]
; AppId fixo: nao altere, e ele que permite atualizar uma instalacao existente
AppId={{6F2B8C1E-4D7A-4E93-9B5C-2A1F0E8D7C34}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Upp Tecnologia
AppPublisherURL=https://upphone.com.br/
AppSupportURL=https://upphone.com.br/
AppUpdatesURL=https://upphone.com.br/
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
SetupIconFile=..\res\microsip.ico
OutputBaseFilename=Upphone-SIP-{#AppVersion}-{#Platform}-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
UsedUserAreasWarning=no
CloseApplications=force
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoCompany=Upp Tecnologia
VersionInfoProductName={#AppName}
#if Platform == "x64"
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion
; GPL v3, endereco do codigo-fonte e componentes de terceiros (especificacao, secao 10)
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "AVISOS.txt"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
; o app (usuario comum) le e apaga o ativacao.ini depois de ativar
Name: "{commonappdata}\{#AppName}"; Permissions: users-modify

[Icons]
; AppUserModelID igual ao do app (jumplist.cpp, _GLOBAL_NAME_VISIBLE): a janela aberta fica no mesmo botao da barra de tarefas que o atalho, com o icone dele
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"; AppUserModelID: "{#AppName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon; AppUserModelID: "{#AppName}"

[Registry]
; O app procura esta chave: se apontar para a pasta do .exe, ele roda em modo "instalado"
; e guarda configuracoes em %APPDATA%\Upphone SIP\ (em vez de ao lado do .exe).
Root: HKLM; Subkey: "Software\{#AppName}"; ValueType: string; ValueName: ""; ValueData: "{app}"; Flags: uninsdeletekey
; O app se registra para iniciar com o Windows; remove essa entrada ao desinstalar
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "{#AppName}"; Flags: uninsdeletevalue dontcreatekey
; Links tel:, sip: e callto: (o app se registra ao abrir, msip_uri_handlers_set em global.cpp); removidos ao desinstalar.
; As classes tel/sip/callto saem em [Code], so quando apontam para o Upphone.
Root: HKCU; Subkey: "Software\Classes\Upphone.Url"; ValueType: none; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\{#AppName}\Capabilities"; ValueType: none; Flags: uninsdeletekey dontcreatekey
Root: HKCU; Subkey: "Software\{#AppName}"; ValueType: none; Flags: uninsdeletekeyifempty dontcreatekey
Root: HKCU; Subkey: "Software\RegisteredApplications"; ValueType: none; ValueName: "{#AppName}"; Flags: uninsdeletevalue dontcreatekey

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
; atualizacao iniciada pelo proprio app (/ATUALIZACAO=1, Provisioning/UpdateDownload.cpp): o instalador fechou o
; app e o abre de novo, como o usuario que pediu (nao como administrador)
Filename: "{app}\{#AppExe}"; Parameters: "/minimized"; Flags: nowait runasoriginaluser; Check: IsAppUpdate

[UninstallDelete]
; sons padrao que o app grava a partir dos embutidos no .exe (EmbeddedSoundFile em mainDlg.cpp)
Type: filesandordirs; Name: "{localappdata}\{#AppName}\sounds"
; codigo de ativacao ainda nao usado
Type: files; Name: "{commonappdata}\{#AppName}\ativacao.ini"
Type: dirifempty; Name: "{commonappdata}\{#AppName}"

[UninstallRun]
Filename: "{sys}\taskkill.exe"; Parameters: "/F /IM {#AppExe}"; Flags: runhidden; RunOnceId: "KillUpphone"

[Code]
// ---------------------------------------------------------------------------------------- ativacao
// Codigo: 16 caracteres do alfabeto Crockford (sem I, L, O, U); o primeiro e o ambiente (P producao,
// H homologacao, D desenvolvimento) e o ultimo um digito verificador (soma ponderada modulo 31). E o
// mesmo algoritmo do app (Provisioning/ProvisioningSupport.cpp) e do servidor (app/security.py).
const
  CodeAlphabet = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';
  ProdServer = 'https://provisionamento.upphone.com.br';
  HomologServer = 'https://homolog.provisionamento.upphone.com.br';

var
  ActivationPage: TInputQueryWizardPage;
  ActivationCode: String; // normalizado; vazio = ativar depois, no app

function ParamValue(const Name: String): String;
begin
  Result := Trim(ExpandConstant('{param:' + Name + '|}'));
end;

function IsAppUpdate(): Boolean;
begin
  Result := ParamValue('ATUALIZACAO') = '1';
end;

function NormalizeCode(Code: String): String;
var
  I: Integer;
  C: String;
  S: String;
begin
  Result := '';
  S := '';
  Code := Uppercase(Code);
  for I := 1 to Length(Code) do
  begin
    C := Copy(Code, I, 1);
    if (C = '-') or (C = ' ') then
      continue;
    if C = 'O' then
      C := '0'
    else if (C = 'I') or (C = 'L') then
      C := '1';
    if Pos(C, CodeAlphabet) = 0 then
      exit;
    S := S + C;
  end;
  if Length(S) = 16 then
    Result := S;
end;

function CodeChecksumOk(const S: String): Boolean;
var
  I, Total: Integer;
begin
  Total := 0;
  for I := 1 to 15 do
    Total := Total + I * (Pos(Copy(S, I, 1), CodeAlphabet) - 1);
  Result := Copy(S, 16, 1) = Copy(CodeAlphabet, (Total mod 31) + 1, 1);
end;

function FormatCode(const S: String): String;
begin
  Result := Copy(S, 1, 4) + '-' + Copy(S, 5, 4) + '-' + Copy(S, 9, 4) + '-' + Copy(S, 13, 4);
end;

function ServerFor(const Code: String): String;
begin
  Result := ParamValue('SERVIDOR');
  if Result = '' then
  begin
    if Copy(Code, 1, 1) = 'P' then
      Result := ProdServer
    else if Copy(Code, 1, 1) = 'H' then
      Result := HomologServer;
  end;
  while (Length(Result) > 0) and (Copy(Result, Length(Result), 1) = '/') do
    Result := Copy(Result, 1, Length(Result) - 1);
end;

function ValidateCode(const Raw: String; var Normalized, Error: String): Boolean;
begin
  Result := False;
  Normalized := NormalizeCode(Raw);
  if Normalized = '' then
    Error := 'O código tem 16 letras e números, no formato XXXX-XXXX-XXXX-XXXX.'
  else if not CodeChecksumOk(Normalized) then
    Error := 'Código digitado incorretamente. Confira os caracteres.'
  else if Pos(Copy(Normalized, 1, 1), 'PHD') = 0 then
    Error := 'Código inválido.'
  else if ServerFor(Normalized) = '' then
    Error := 'Código de desenvolvimento: informe o servidor com /SERVIDOR=.'
  else
    Result := True;
end;

// valor de texto de um JSON simples (as respostas do servidor vem so em ASCII, acentos como ç)
function JsonString(const Json, Key: String): String;
var
  P, I: Integer;
  C: String;
begin
  Result := '';
  P := Pos('"' + Key + '":"', Json);
  if P = 0 then
    exit;
  I := P + Length(Key) + 4;
  while I <= Length(Json) do
  begin
    C := Copy(Json, I, 1);
    if C = '"' then
      break;
    if (C = '\') and (I < Length(Json)) then
    begin
      I := I + 1;
      C := Copy(Json, I, 1);
      if (C = 'u') and (I + 4 <= Length(Json)) then
      begin
        C := Chr(StrToInt('$' + Copy(Json, I + 1, 4)));
        I := I + 4;
      end
      else if (C = 'n') or (C = 'r') or (C = 't') then
        C := ' ';
    end;
    Result := Result + C;
    I := I + 1;
  end;
end;

// Confere o codigo no servidor sem usa-lo. 1 valido (Info = resposta), 0 recusado, -1 servidor inalcancavel.
// O WinHttpRequest valida o certificado HTTPS.
function CheckCodeOnServer(const Server, Code: String; var Info: String): Integer;
var
  Http: Variant;
begin
  Info := '';
  try
    Http := CreateOleObject('WinHttp.WinHttpRequest.5.1');
    Http.Open('POST', Server + '/api/v1/activation/check', False);
    Http.SetTimeouts(10000, 10000, 15000, 15000);
    Http.SetRequestHeader('Content-Type', 'application/json');
    Http.Send('{"code":"' + Code + '"}');
    Info := Http.ResponseText;
    if Http.Status = 200 then
      Result := 1
    else if (Http.Status >= 400) and (Http.Status < 500) then
      Result := 0
    else
      Result := -1;
  except
    Result := -1;
  end;
end;

function InitializeSetup(): Boolean;
var
  Server: String;
begin
  Result := True;
  Server := Lowercase(ParamValue('SERVIDOR'));
  // so https (ou http do proprio computador, para desenvolvimento)
  if (Server <> '') and (Pos('https://', Server) <> 1) and (Pos('http://127.0.0.1', Server) <> 1)
     and (Pos('http://localhost', Server) <> 1) then
  begin
    SuppressibleMsgBox('O servidor de provisionamento (/SERVIDOR) precisa usar https://.', mbError, MB_OK, IDOK);
    Result := False;
  end;
end;

procedure InitializeWizard;
begin
  ActivationPage := CreateInputQueryPage(wpSelectTasks, 'Ativação', 'Código de ativação enviado pela Upp',
    'Digite o código para o Upphone SIP abrir já com o seu ramal. Ele vale por pouco tempo e só pode ser usado uma vez.' + #13#10#13#10 +
    'Se ainda não tem o código, deixe em branco: o Upphone pede o código quando abrir.');
  ActivationPage.Add('Código de ativação (XXXX-XXXX-XXXX-XXXX):', False);
  ActivationPage.Values[0] := ParamValue('CODIGO');
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  Normalized, Error, Info, Text: String;
  Status: Integer;
begin
  Result := True;
  if CurPageID <> ActivationPage.ID then
    exit;
  ActivationCode := '';
  if Trim(ActivationPage.Values[0]) = '' then
    exit;
  if not ValidateCode(ActivationPage.Values[0], Normalized, Error) then
  begin
    SuppressibleMsgBox(Error, mbError, MB_OK, IDOK);
    Result := WizardSilent(); // na instalacao silenciosa segue sem codigo
    exit;
  end;
  ActivationPage.Values[0] := FormatCode(Normalized);
  if not WizardSilent() then
  begin
    // validacao antes de instalar: o servidor confere o codigo (sem usa-lo) e diz de qual ramal e
    Status := CheckCodeOnServer(ServerFor(Normalized), FormatCode(Normalized), Info);
    if Status = 0 then
    begin
      Text := JsonString(Info, 'message');
      if Text = '' then
        Text := 'Código recusado pela Upp.';
      MsgBox(Text, mbError, MB_OK);
      Result := False;
      exit;
    end;
    if Status = 1 then
      MsgBox('Código válido: ' + JsonString(Info, 'extension') + ' (' + JsonString(Info, 'client') + ').' + #13#10 +
        'O Upphone SIP ativa este ramal quando abrir.', mbInformation, MB_OK)
    else if MsgBox('Não foi possível conferir o código com a Upp agora. Continuar mesmo assim?' + #13#10 +
        'O Upphone SIP tenta ativar quando abrir.', mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
      exit;
    end;
  end;
  ActivationCode := Normalized;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  FileName, Error, Normalized: String;
begin
  if CurStep <> ssPostInstall then
    exit;
  // instalacao silenciosa sem passar pela pagina: o codigo do parametro, conferido so no formato
  if (ActivationCode = '') and WizardSilent() and (ParamValue('CODIGO') <> '') then
    if ValidateCode(ParamValue('CODIGO'), Normalized, Error) then
      ActivationCode := Normalized
    else
      Log('Codigo de ativacao ignorado: ' + Error);
  if ActivationCode <> '' then
  begin
    FileName := ExpandConstant('{commonappdata}\{#AppName}\ativacao.ini');
    DeleteFile(FileName);
    SetIniString('Ativacao', 'codigo', FormatCode(ActivationCode), FileName);
    if ParamValue('SERVIDOR') <> '' then
      SetIniString('Ativacao', 'servidor', ServerFor(ActivationCode), FileName);
    if ParamValue('USUARIO') <> '' then
      SetIniString('Ativacao', 'usuario', ParamValue('USUARIO'), FileName);
    Log('Codigo de ativacao ...' + Copy(ActivationCode, 13, 4) + ' deixado para o app');
  end;
  if ParamValue('SERVIDOR') <> '' then
    RegWriteStringValue(HKLM, 'Software\{#AppName}', 'ProvisioningServer', ServerFor('X'));
end;

// Remove os links tel:, sip: e callto: que apontam para o Upphone (os de outros programas ficam)
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Protocols: array[0..2] of String;
  Command: String;
  I: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    Protocols[0] := 'tel';
    Protocols[1] := 'sip';
    Protocols[2] := 'callto';
    for I := 0 to 2 do
      if RegQueryStringValue(HKCU, 'Software\Classes\' + Protocols[I] + '\shell\open\command', '', Command) and
         (Pos('upphone.exe', Lowercase(Command)) > 0) then
        RegDeleteKeyIncludingSubkeys(HKCU, 'Software\Classes\' + Protocols[I]);
  end;
end;
