#include "dsnConfigForm.hpp"

#include "../../trinoAPIWrapper/connectionConfig.hpp"
#include "../../trinoAPIWrapper/trinoQuery.hpp"
#include "../../util/stringFromChar.hpp"
#include "../../util/writeLog.hpp"
#include "win32controls/buttonMaker.hpp"
#include "win32controls/comboboxMaker.hpp"
#include "win32controls/editMaker.hpp"
#include "win32controls/labelMaker.hpp"

#include <CommCtrl.h>

// Define some identifiers for buttons and fields in the edit form.
constexpr int ID_EDIT_DSN             = 101;
constexpr int ID_EDIT_HOSTNAME        = 102;
constexpr int ID_EDIT_PORT            = 103;
constexpr int ID_BUTTON_SAVE          = 104;
constexpr int ID_BUTTON_CANCEL        = 105;
constexpr int ID_COMBO_LOGLEVEL       = 106;
constexpr int ID_COMBO_AUTHMETHOD     = 107;
constexpr int ID_EDIT_OIDC_DISC       = 108;
constexpr int ID_EDIT_CLIENT_ID       = 109;
constexpr int ID_EDIT_CLIENT_SECRET   = 110;
constexpr int ID_EDIT_OIDC_SCOPE      = 111;
constexpr int ID_STATIC_OIDC_DISC     = 112;
constexpr int ID_STATIC_CLIENT_ID     = 113;
constexpr int ID_STATIC_CLIENT_SECRET = 114;
constexpr int ID_STATIC_OIDC_SCOPE    = 115;
constexpr int ID_EDIT_USER            = 116;
constexpr int ID_EDIT_PASSWORD        = 117;
constexpr int ID_BUTTON_TEST          = 118;
constexpr int BUF_LEN                 = 1024;

// Vertical position of the button row.
constexpr int BUTTON_ROW_Y = 350;


std::string NOT_REQUIRED = "<Not Required>";

// Simple constructor that saves a copy of the parent window handle.
DSNForm::DSNForm(HWND parent, std::map<std::string, std::string> attributes) {
  this->parent = parent;
  if (attributes.count("DSN") > 0) {
    this->configResult.setDSN(attributes.at("DSN"));
  }
  if (attributes.count("hostname") > 0) {
    this->configResult.setHostname(attributes.at("hostname"));
  }
  if (attributes.count("port") > 0) {
    this->configResult.setPort(attributes.at("port"));
  }
  if (attributes.count("loglevel") > 0) {
    this->configResult.setLogLevel(attributes.at("loglevel"));
  }
  if (attributes.count("authmethod") > 0) {
    this->configResult.setAuthMethod(attributes.at("authmethod"));
  }
  if (attributes.count("oidcDiscoveryUrl") > 0) {
    this->configResult.setOidcDiscoveryUrl(attributes.at("oidcDiscoveryUrl"));
  }
  if (attributes.count("clientId") > 0) {
    this->configResult.setClientId(attributes.at("clientId"));
  }
  if (attributes.count("clientSecret") > 0) {
    this->configResult.setClientSecret(attributes.at("clientSecret"));
  }
  if (attributes.count("oidcScope") > 0) {
    this->configResult.setOidcScope(attributes.at("oidcScope"));
  }
  if (attributes.count("uid") > 0) {
    this->configResult.setUser(attributes.at("uid"));
  }
}

// Read a text field, treating the "<Not Required>" placeholder as empty.
static std::string readField(HWND item) {
  std::string text = getEditText(item);
  return text == NOT_REQUIRED ? "" : text;
}

// Copy every value currently shown in the form into a DriverConfig.
static void readFormIntoConfig(HWND hwnd, DriverConfig* config) {
  char buf[BUF_LEN] = {0};

  config->setDSN(getEditText(GetDlgItem(hwnd, ID_EDIT_DSN)));
  config->setHostname(getEditText(GetDlgItem(hwnd, ID_EDIT_HOSTNAME)));
  config->setPort(getEditText(GetDlgItem(hwnd, ID_EDIT_PORT)));

  HWND logLevelItem = GetDlgItem(hwnd, ID_COMBO_LOGLEVEL);
  LRESULT logIndex  = SendMessage(logLevelItem, CB_GETCURSEL, 0, 0);
  SendMessage(logLevelItem, CB_GETLBTEXT, logIndex, (LPARAM)buf);
  config->setLogLevel(std::string(buf));

  HWND authMethodItem = GetDlgItem(hwnd, ID_COMBO_AUTHMETHOD);
  LRESULT authIndex   = SendMessage(authMethodItem, CB_GETCURSEL, 0, 0);
  SendMessage(authMethodItem, CB_GETLBTEXT, authIndex, (LPARAM)buf);
  config->setAuthMethod(std::string(buf));

  config->setOidcDiscoveryUrl(readField(GetDlgItem(hwnd, ID_EDIT_OIDC_DISC)));
  config->setClientId(readField(GetDlgItem(hwnd, ID_EDIT_CLIENT_ID)));
  config->setClientSecret(readField(GetDlgItem(hwnd, ID_EDIT_CLIENT_SECRET)));
  config->setOidcScope(readField(GetDlgItem(hwnd, ID_EDIT_OIDC_SCOPE)));
  config->setUser(getEditText(GetDlgItem(hwnd, ID_EDIT_USER)));
  config->setPassword(getEditText(GetDlgItem(hwnd, ID_EDIT_PASSWORD)));
}

// Run "SELECT current_user" with the values in the form and report the
// outcome in a message box. Nothing is written to the server.
static void testConnection(HWND hwnd) {
  DriverConfig config;
  HCURSOR oldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
  std::string message;
  UINT icon = MB_ICONERROR;

  try {
    readFormIntoConfig(hwnd, &config);
    if (config.getHostname().empty()) {
      throw std::invalid_argument("Hostname is empty.");
    }
    ConnectionConfig connectionConfig(config.getHostname(),
                                      config.getPortNum(),
                                      config.getAuthMethodEnum(),
                                      config.getDSN(),
                                      config.getOidcDiscoveryUrl(),
                                      config.getClientId(),
                                      config.getClientSecret(),
                                      config.getOidcScope(),
                                      config.getGrantType(),
                                      config.getTokenEndpoint(),
                                      config.getUser(),
                                      config.getPassword());
    std::string currentUser;
    {
      TrinoQuery query(&connectionConfig);
      query.setQuery("SELECT current_user");
      query.post();
      query.poll(ToCompletion);
      if (query.hasError()) {
        throw std::runtime_error(query.getError().message);
      }
      if (query.getCurrentRowCount() > 0) {
        const json& row = query.getRowAtIndex(0);
        if (row.is_array() && !row.empty() && row[0].is_string()) {
          currentUser = row[0].get<std::string>();
        }
      }
    }
    std::string version = connectionConfig.getTrinoServerVersion();
    connectionConfig.disconnect();

    message = "Connection succeeded.\n\nServer: " + config.getHostname() +
              ":" + config.getPortStr() + "\nTrino version: " +
              (version.empty() ? "(unknown)" : version) +
              "\nConnected as: " +
              (currentUser.empty() ? "(unknown)" : currentUser);
    icon = MB_ICONINFORMATION;
  } catch (std::exception& e) {
    message = std::string("Connection failed.\n\n") + e.what();
  }

  SetCursor(oldCursor);
  WriteLog(LL_INFO, "  Test connection result: " + message);
  MessageBox(hwnd, message.c_str(), "Trino ODBC - Test Connection", MB_OK | icon);
}


LRESULT CALLBACK WINDOW_CB(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  // This is the window "procedure" for the DSN Config window, which is
  // essentially a callback function. This is where we interact with the window
  // to extract values from it.
  DriverConfig* driverConfigPtr = nullptr;
  if (uMsg == WM_CREATE) {
    CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
    driverConfigPtr = reinterpret_cast<DriverConfig*>(pCreate->lpCreateParams);
    SetWindowLongPtr(
        hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(driverConfigPtr));
  } else {
    driverConfigPtr =
        reinterpret_cast<DriverConfig*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  switch (uMsg) {
    case WM_COMMAND: {
      WriteLog(LL_TRACE, "  WM_COMMAND executing");
      HWND dsnItem               = GetDlgItem(hwnd, ID_EDIT_DSN);
      HWND hostnameItem          = GetDlgItem(hwnd, ID_EDIT_HOSTNAME);
      HWND portItem              = GetDlgItem(hwnd, ID_EDIT_PORT);
      HWND logLevelItem          = GetDlgItem(hwnd, ID_COMBO_LOGLEVEL);
      HWND authMethodItem        = GetDlgItem(hwnd, ID_COMBO_AUTHMETHOD);
      HWND oidcDiscoveryUrlItem  = GetDlgItem(hwnd, ID_EDIT_OIDC_DISC);
      HWND oidcDiscoveryUrlLabel = GetDlgItem(hwnd, ID_STATIC_OIDC_DISC);
      HWND clientIdItem          = GetDlgItem(hwnd, ID_EDIT_CLIENT_ID);
      HWND clientIdLabel         = GetDlgItem(hwnd, ID_STATIC_CLIENT_ID);
      HWND clientSecretItem      = GetDlgItem(hwnd, ID_EDIT_CLIENT_SECRET);
      HWND clientSecretLabel     = GetDlgItem(hwnd, ID_STATIC_CLIENT_SECRET);
      HWND oidcScopeItem         = GetDlgItem(hwnd, ID_EDIT_OIDC_SCOPE);
      HWND oidcScopeLabel        = GetDlgItem(hwnd, ID_STATIC_OIDC_SCOPE);
      char buf[BUF_LEN]          = {0};
      switch (LOWORD(wParam)) {
        case ID_BUTTON_SAVE: {
          WriteLog(LL_TRACE, "  ID_BUTTON_SAVE executing");
          readFormIntoConfig(hwnd, driverConfigPtr);

          driverConfigPtr->setIsSaved(true);

          DestroyWindow(hwnd);
          break;
        }
        case ID_BUTTON_TEST: {
          WriteLog(LL_TRACE, "  ID_BUTTON_TEST executing");
          testConnection(hwnd);
          break;
        }
        case ID_BUTTON_CANCEL: {
          WriteLog(LL_TRACE, "  ID_BUTTON_CANCEL executing");
          driverConfigPtr->setIsSaved(false);

          DestroyWindow(hwnd);
          break;
        }
        default: {
          WriteLog(LL_TRACE, "  WINDOW_CB default (redraw)");
          LRESULT authIndex = SendMessage(authMethodItem, CB_GETCURSEL, 0, 0);
          SendMessage(authMethodItem, CB_GETLBTEXT, authIndex, (LPARAM)buf);
          std::string authMethod = stringFromChar(buf, CHAR_IS_NTS);
          if (authMethod == "OIDC Client Cred Auth") {
            WriteLog(LL_TRACE, "  Client Cred Auth");
            // Show the OIDC Config fields
            if (getEditText(oidcDiscoveryUrlItem) == NOT_REQUIRED) {
              setEditText(oidcDiscoveryUrlItem, "");
            }
            setEditWriteable(oidcDiscoveryUrlItem);
            if (getEditText(clientIdItem) == NOT_REQUIRED) {
              setEditText(clientIdItem, "");
            }
            setEditWriteable(clientIdItem);
            if (getEditText(clientSecretItem) == NOT_REQUIRED) {
              setEditText(clientSecretItem, "");
            }
            setEditWriteable(clientSecretItem);
            if (getEditText(oidcScopeItem) == NOT_REQUIRED) {
              setEditText(oidcScopeItem, "");
            }
            setEditWriteable(oidcScopeItem);
            InvalidateRect(hwnd, NULL, FALSE);
          } else if (not authMethod.empty()) {
            WriteLog(LL_TRACE, "  Other Auth");
            // Hide the OIDC Config fields
            if (getEditText(oidcDiscoveryUrlItem) == "") {
              setEditText(oidcDiscoveryUrlItem, NOT_REQUIRED);
            }
            setEditReadOnly(oidcDiscoveryUrlItem);
            if (getEditText(clientIdItem) == "") {
              setEditText(clientIdItem, NOT_REQUIRED);
            }
            setEditReadOnly(clientIdItem);
            if (getEditText(clientSecretItem) == "") {
              setEditText(clientSecretItem, NOT_REQUIRED);
            }
            setEditReadOnly(clientSecretItem);
            if (getEditText(oidcScopeItem) == "") {
              setEditText(oidcScopeItem, NOT_REQUIRED);
            }
            setEditReadOnly(oidcScopeItem);
            InvalidateRect(hwnd, NULL, FALSE);
          }
          break;
        }
      }
      break;
    }
    case WM_DESTROY: {
      WriteLog(LL_TRACE, "  WM_DESTROY executing");
      PostQuitMessage(0);
      break;
    }
    default: {
      return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
  }
  return 0;
}

void DSNForm::ShowDSNForm() {
  WriteLog(LL_DEBUG, "  Creating DSN Form");
  const char className[] = "FormWindowClass";

  WNDCLASS wc      = {};
  wc.lpfnWndProc   = WINDOW_CB;
  wc.hInstance     = GetModuleHandle(NULL);
  wc.lpszClassName = className;
  // Getting data back from a window callback is tricky. The easiest way
  // to do it is to share a pointer.
  wc.cbWndExtra = sizeof(DriverConfig*);

  WriteLog(LL_TRACE, "  Registering class");
  RegisterClass(&wc);

  WriteLog(LL_TRACE, "  Creating window");
  HWND form    = CreateWindowEx(WS_EX_CLIENTEDGE,
                             className,
                             "Enter DSN Information",
                             WS_OVERLAPPED | WS_VISIBLE,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             625,
                             440,
                             this->parent,
                             NULL,
                             GetModuleHandle(NULL),
                             &this->configResult);
  bool visible = true;
  // FEATURE: actually hide the OIDC fields if the user doesn't select OIDC
  // auth. For now, they're just disabled but still visible.
  bool oidcVis = true;

  WriteLog(LL_TRACE, "  Creating Labels");
  labelMaker(form, 10, NULL, "DSN Name:", visible);
  labelMaker(form, 40, NULL, "Hostname:", visible);
  labelMaker(form, 70, NULL, "Port:", visible);
  labelMaker(form, 100, NULL, "Log Level:", visible);
  labelMaker(form, 130, NULL, "Auth Method:", visible);
  labelMaker(form, 160, ID_STATIC_OIDC_DISC, "OIDC Discovery URL:", oidcVis);
  labelMaker(form, 190, ID_STATIC_CLIENT_ID, "Client ID:", oidcVis);
  labelMaker(form, 220, ID_STATIC_CLIENT_SECRET, "Client Secret:", oidcVis);
  labelMaker(form, 250, ID_STATIC_OIDC_SCOPE, "OIDC Scope:", oidcVis);
  labelMaker(form, 280, NULL, "User (UID):", visible);
  labelMaker(form, 310, NULL, "Password:", visible);

  WriteLog(LL_TRACE, "  Creating Text Entries");
  HWND hwndDsn              = editMaker(form, 10, ID_EDIT_DSN, visible);
  HWND hwndHostname         = editMaker(form, 40, ID_EDIT_HOSTNAME, visible);
  HWND hwndPort             = editMaker(form, 70, ID_EDIT_PORT, visible);
  HWND hwndOidcDiscoveryUrl = editMaker(form, 160, ID_EDIT_OIDC_DISC, oidcVis);
  HWND hwndClientId         = editMaker(form, 190, ID_EDIT_CLIENT_ID, oidcVis);
  HWND hwndClientSecret = editMaker(form, 220, ID_EDIT_CLIENT_SECRET, oidcVis);
  HWND hwndOidcScope    = editMaker(form, 250, ID_EDIT_OIDC_SCOPE, oidcVis);
  HWND hwndUser         = editMaker(form, 280, ID_EDIT_USER, visible);
  // The password box cannot use editMaker: ES_PASSWORD does not work together
  // with ES_MULTILINE.
  HWND hwndPassword = CreateWindow(
      "EDIT",
      "",
      WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP | ES_PASSWORD |
          ES_AUTOHSCROLL,
      165,
      310,
      425,
      20,
      form,
      reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ID_EDIT_PASSWORD)),
      GetModuleHandle(NULL),
      NULL);

  // Sometimes the DSN should be read-only such as when an existing DSN is being
  // configured. In those cases, we need to set the EDIT control to readonly as
  // well.
  if (this->readOnlyDSN) {
    setEditReadOnly(hwndDsn);
  }

  WriteLog(LL_TRACE, "  Creating Log Level Combobox");
  HWND hwndLogLevel =
      comboboxMaker(form, 100, ID_COMBO_LOGLEVEL, LOG_LEVEL_NAMES);

  WriteLog(LL_TRACE, "  Creating Auth Method Combobox");
  HWND hwndAuthMethod =
      comboboxMaker(form, 130, ID_COMBO_AUTHMETHOD, AUTH_METHOD_NAMES);

  WriteLog(LL_TRACE, "  Pre-populating the text fields");
  setEditText(hwndDsn, this->configResult.getDSN());
  setEditText(hwndHostname, this->configResult.getHostname());
  setEditText(hwndPort, this->configResult.getPortStr());
  setEditText(hwndOidcDiscoveryUrl, this->configResult.getOidcDiscoveryUrl());
  setEditText(hwndClientId, this->configResult.getClientId());
  setEditText(hwndClientSecret, this->configResult.getClientSecret());
  setEditText(hwndOidcScope, this->configResult.getOidcScope());
  setEditText(hwndUser, this->configResult.getUser());
  setEditText(hwndPassword, this->configResult.getPassword());

  WriteLog(LL_TRACE, "  Pre-poplating Comboboxes");
  setCombobox(hwndLogLevel, this->configResult.getLogLevelStr());
  setCombobox(hwndAuthMethod, this->configResult.getAuthMethodStr());

  WriteLog(LL_TRACE, "  Creating Buttons");
  HWND hwndTest =
      buttonMaker(form, 165, ID_BUTTON_TEST, "Test Connection", BUTTON_ROW_Y, 130);
  HWND hwndSave   = buttonMaker(form, 310, ID_BUTTON_SAVE, "Save", BUTTON_ROW_Y);
  HWND hwndCancel =
      buttonMaker(form, 405, ID_BUTTON_CANCEL, "Cancel", BUTTON_ROW_Y);

  MSG msg = {};
  WriteLog(LL_TRACE, "  Polling...");
  while (GetMessage(&msg, NULL, 0, 0)) {
    if (!IsDialogMessage(form, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  WriteLog(LL_TRACE, "  Polling completed");

  WriteLog(LL_TRACE, "  Unregistering class");
  UnregisterClass(className, GetModuleHandle(NULL));
}

DriverConfig DSNForm::getResult() {
  return this->configResult;
}

void DSNForm::updateConfig(DriverConfig config) {
  this->configResult = config;
}

void DSNForm::setReadOnlyDSN() {
  this->readOnlyDSN = true;
}
