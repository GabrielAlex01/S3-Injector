# S3-Injector

Dispositivo USB composto para exercicios de **Purple Team** baseado no **ESP32-S3**. Combina teclado HID (injecao de keystrokes via DuckyScript) com porta serial CDC (exfiltracao de dados), controlado por uma interface web autenticada via Wi-Fi.

> **Uso exclusivo em exercicios autorizados.**

## Arquitetura

```
ESP32-S3
  USB HID Keyboard   Digita comandos no alvo (DuckyScript)
  USB CDC Serial      Recebe dados exfiltrados do alvo
  Wi-Fi AP (10.0.0.1)
    Interface Web (React)   Operador controla tudo pelo navegador
```

- **TinyUSB** em modo composto (HID + CDC) com `ARDUINO_USB_MODE=0`
- **Spoofing USB** como dispositivo Microsoft (VID `0x045E`)
- **ABNT2** como layout de teclado para digitacao
- **FreeRTOS mutex** para thread safety entre Core 0 (web) e Core 1 (USB)
- **Autenticacao por token** com login obrigatorio no painel web, token unico gerado a cada boot

## Hardware

O projeto foi inicialmente desenvolvido para o **ESP32-S3 DevKitC-1** padrao e depois focado no **M5Stack ATOM S3 Lite** por seu tamanho reduzido e discricao, ideal para exercicios e simulacoes. Ambos utilizam o mesmo microcontrolador **ESP32-S3**, entao o firmware e totalmente compativel.

- **ESP32-S3** (flash 8MB, USB OTG nativo)
- Cabo USB-C com dados (nao apenas carga)
- Preferencialmente use uma **porta USB 3.0** pois a velocidade de enumeracao e superior e o dispositivo e reconhecido mais rapidamente pelo sistema operacional do alvo

## Notebook vs Desktop

O macro `ADMIN_PS_OPEN` em `src/payloads.h` abre o PowerShell como Administrador navegando pelo menu Win+X. Por padrao esta configurado para **notebook** com `REPEAT 9`.

**Para desktop, altere para `REPEAT 8`**, pois o menu Win+X tem uma opcao a menos (nao possui as opcoes de energia do notebook).

## Prerequisitos

- [PlatformIO](https://platformio.org/install) (CLI ou extensao VS Code)
- [Node.js](https://nodejs.org/) 18+ (para build do frontend)
- [Python](https://www.python.org/) 3.8+ (para embed do frontend)

## Instalacao

### 1. Clonar o repositorio

```bash
git clone https://github.com/SEU_USUARIO/S3-Injector.git
cd S3-Injector
```

### 2. Instalar dependencias do frontend

```bash
cd frontend
npm install
cd ..
```

### 3. Build do frontend

Compila o React em arquivos estaticos:

```bash
cd frontend
npm run build
cd ..
```

Os arquivos sao gerados em `data/www/` (index.html, app.js, app.css).

### 4. Embeder frontend no firmware

Converte os arquivos em arrays C comprimidos com gzip:

```bash
python embed_frontend.py
```

Gera o arquivo `src/web_content.h` com os assets embutidos.

### 5. Compilar o firmware

```bash
pio run
```

### 6. Gravar no ESP32-S3

Conecte o ESP32-S3 via USB e:

```bash
pio run -t upload
```

Se a porta nao for detectada automaticamente:

```bash
pio run -t upload --upload-port COM6
```

### 7. Monitorar serial (debug)

```bash
pio device monitor
```

## Uso

### Conectar ao dispositivo

1. Conecte o S3-Injector na **porta USB 3.0** do alvo (preferencialmente)
2. No celular ou notebook, conecte no Wi-Fi:
   - **SSID:** `S3-Injector`
   - **Senha:** `S3-Payloads@!`
3. Acesse `http://10.0.0.1` no navegador
4. Faca login com as credenciais configuradas

### Interface Web

- **Login** Autenticacao obrigatoria antes de acessar o painel
- **Status Bar** Indica se o USB esta conectado ao alvo (OTG ON/OFF)
- **Payloads** Lista os payloads disponiveis para execucao
- **Loot Dashboard** Exibe dados exfiltrados via serial CDC

### Payloads Built-in

| Payload | Funcao |
|---------|--------|
| Recon Basico | `whoami`, hostname e `ipconfig /all` |
| Recon Defesas | Processos de EDR/AV, Zabbix Agent, Defender |
| Recon Rede | Tabela ARP, conexoes ativas e rotas |
| Exfil Wi-Fi | SSIDs salvos e senhas |
| Enum Portas | Portas em escuta, scan do gateway e regras de firewall Allow em portas criticas |
| Diag COM | Lista portas COM e dispositivos serial (filtra Bluetooth) |
| Diminuir Defesas | Exclusao no Defender e regras de firewall na porta 4444 |
| Reverter Defesas | Reverte exclusoes e regras criadas pelo Diminuir Defesas |
| Enum Admins | Usuario atual, membros do grupo Administradores e ultimo logon |
| Criar Admin | Cria usuario `S3Injector` (senha `Senha123!`) como administrador local |

### Custom Payloads

Clique em **"+ Novo Payload"** na aba Payloads para adicionar DuckyScript personalizado. Tambem aceita upload de arquivos `.txt`.

Exemplo de DuckyScript:

```
GUI r
DELAY 500
STRING notepad
ENTER
DELAY 1000
STRING Bom Dia!
```

## Estrutura do Projeto

```
src/
  main.cpp            Firmware principal
  config.h            Configuracoes (Wi-Fi, USB, auth, timing)
  payloads.h          Payloads DuckyScript built-in
  ducky.h             Engine DuckyScript com ABNT2
  web_content.h       Frontend embutido (gerado)
frontend/
  src/
    App.tsx            Aplicacao React
    api.ts             Cliente API com autenticacao
    index.css          Estilos
    components/
      LoginModal.tsx
      StatusBar.tsx
      TriggerPanel.tsx
      LootDashboard.tsx
embed_frontend.py      Script de conversao frontend para C arrays
platformio.ini         Configuracao PlatformIO
```

## Configuracao

Edite `src/config.h` para alterar:

| Parametro | Padrao | Descricao |
|-----------|--------|-----------|
| `AP_SSID` | `S3-Injector` | Nome da rede Wi-Fi |
| `AP_PASSWORD` | `S3-Payloads@!` | Senha do Wi-Fi |
| `AP_CHANNEL` | `6` | Canal Wi-Fi |
| `AP_HIDDEN` | `false` | Ocultar SSID |
| `AP_LOCAL_IP` | `10.0.0.1` | IP do dispositivo |
| `AUTH_USER` | `admin` | Usuario do painel web |
| `AUTH_PASS` | `admin` | Senha do painel web |
| `C2_USB_VID` | `0x045E` | USB Vendor ID |
| `C2_USB_PID` | `0x07B9` | USB Product ID |
| `C2_USB_MFR` | `Microsoft` | Fabricante USB |
| `C2_USB_PRODUCT` | `USB Input Device` | Nome do produto USB |

## Build Pipeline Completo

Para qualquer alteracao no frontend ou payloads:

```bash
cd frontend && npm run build && cd ..
python embed_frontend.py
pio run -t upload
```

## API Endpoints

Todas as rotas `/api/*` (exceto `/api/login`) exigem header `Authorization: Bearer <token>`.

| Metodo | Rota | Funcao |
|--------|------|--------|
| POST | `/api/login` | Autenticacao `{"user":"...","pass":"..."}` |
| GET | `/api/status` | Status do dispositivo |
| GET | `/api/loot` | Lista loot capturado |
| POST | `/api/loot/clear` | Limpa todo o loot |
| POST | `/api/loot/test` | Adiciona loot de teste |
| POST | `/api/loot/receive` | Recebe loot via Wi-Fi |
| GET | `/api/payloads` | Lista payloads |
| POST | `/api/execute` | Executa payload `{"payload":"id"}` |
| POST | `/api/payloads/create` | Cria payload custom |
| POST | `/api/payloads/delete?id=N` | Remove payload custom |

## Troubleshooting

**ESP32-S3 nao aparece como porta COM:**
Certifique-se de usar um cabo USB-C com dados. Alguns cabos sao apenas de carga.

**Keystrokes digitam caracteres errados:**
O engine usa layout ABNT2. Se o alvo usa outro layout, os caracteres especiais sairao incorretos.

**OTG mostra OFF mesmo conectado:**
Verifique se o driver CDC foi reconhecido pelo sistema. No Windows, deve aparecer em Gerenciador de Dispositivos > Portas.

**Frontend nao carrega:**
Execute o build pipeline completo (build frontend, embed, compile, upload).

**Payloads nao abrem como Admin no desktop:**
Altere `REPEAT 9` para `REPEAT 8` em `src/payloads.h` (ver secao "Notebook vs Desktop").
