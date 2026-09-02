import { bullets, callout, code, paragraph, screenshot, steps, table, type DocPage } from "./types";

export const authPages: DocPage[] = [
  {
    slug: "authentication/otp-mfa",
    title: "OTP and MFA login",
    eyebrow: "Authentication / Blueprint",
    description: "Request a one-time password, complete the login, and handle an MFA continuation without treating it as a failed request.",
    readTime: "8 min",
    sections: [
      {
        id: "auth-collection",
        title: "Start from an auth collection reference",
        blocks: [
          paragraph(
            "Connect Use Collection to the auth action and pick the collection from your imported schema. An auth-filtered picker accepts sdk_users and keeps base collections out of this flow."
          ),
          screenshot("[put a screenshot of sdk_users connected to List Authentication Methods here]"),
          callout("note", "List methods when the sign-in UI changes", "List Authentication Methods returns the password, OTP, MFA, and OAuth2 settings enabled by the server. Build the available sign-in buttons from that response when backend configuration can change."),
        ],
      },
      {
        id: "otp",
        title: "One-time password flow",
        blocks: [
          steps(
            { title: "Request the code", text: "Call Request One-Time Password with the user's email. Success returns OtpId." },
            { title: "Collect the code", text: "Ask the user for the code delivered by the configured PocketBase mail flow." },
            { title: "Authenticate", text: "Call Log In with One-Time Password using OtpId and One-Time Password. Supply an MFA continuation only when this attempt completes an MFA challenge." },
            { title: "Read the terminal path", text: "The result either authenticates, requests MFA, fails, or gets cancelled. Exactly one execution output fires." }
          ),
          screenshot("[put a screenshot of Request One-Time Password followed by Log In with One-Time Password here]"),
          callout("warning", "Keep the two values separate", "OtpId identifies the server request. One-Time Password is the delivered login code. They aren't interchangeable."),
        ],
      },
      {
        id: "mfa",
        title: "MFA continues the sign-in attempt",
        blocks: [
          paragraph(
            "Password, OTP, and OAuth2 can fire Mfa Required after the server accepts the first credential. Keep the returned continuation for the short follow-up flow and pass it into the next supported auth action. The client isn't authenticated till that follow-up succeeds."
          ),
          bullets(
            "Don't treat Mfa Required as an authenticated session.",
            "Keep the continuation only for the short-lived follow-up flow.",
            "Don't persist the continuation in config, saves, or logs.",
            "Pass it through the Mfa input on the next supported auth action."
          ),
        ],
      },
    ],
  },
  {
    slug: "authentication/oauth2",
    title: "OAuth2, manual and assisted",
    eyebrow: "Authentication",
    description: "Use the browser-neutral manual flow by default, understand PKCE state ownership, and enable assisted OAuth only after a packaged provider path is configured.",
    readTime: "13 min",
    sections: [
      {
        id: "choose-flow",
        title: "Choose the flow deliberately",
        blocks: [
          table(
            ["Flow", "Best for", "SDK responsibility"],
            [
              ["Manual OAuth2", "Any app that owns its redirect and browser handoff", "Creates PKCE transaction, validates state and callback, exchanges code"],
              ["Assisted OAuth2", "A packaged platform with a proven browser bridge and realtime handoff", "Discovers provider, subscribes to @oauth2, opens browser, exchanges returned code"]
            ]
          ),
          callout("success", "Manual is the safe default", "It is browser-neutral and leaves redirect capture in application code, where platform URL schemes and provider configuration already belong."),
        ],
      },
      {
        id: "manual",
        title: "Manual OAuth2 flow",
        blocks: [
          steps(
            { title: "List providers", text: "Call List Authentication Methods and choose one OAuth2 provider name returned by PocketBase." },
            { title: "Begin", text: "Call Begin Manual OAuth2 with Provider, the exact app-owned RedirectUrl, optional scopes, and request options." },
            { title: "Open", text: "Open AuthorizationUrl using platform or application code. Keep TransactionId with this one attempt." },
            { title: "Capture", text: "Your application receives the configured callback URL after the provider redirects." },
            { title: "Complete", text: "Call Complete Manual OAuth2 with TransactionId, the full callback URL, optional create data, and any MFA continuation." }
          ),
          screenshot("[put a screenshot of Begin Manual OAuth2 outputs feeding an app browser open step here]"),
          screenshot("[put a screenshot of an app callback URL feeding Complete Manual OAuth2 here]"),
          code(
            "cpp",
            `FOpenPocketBaseOAuth2StartOptions Start;
Start.Provider = TEXT("github");
Start.RedirectUrl = TEXT("mygame://oauth/callback");

Auth.BeginOAuth2(MoveTemp(Start),
    [Auth](TOpenPocketBaseResult<FOpenPocketBaseOAuth2Authorization>&& Begun)
    {
        if (Begun.IsSuccess())
        {
            OpenApplicationBrowser(Begun.GetValue().AuthorizationUrl);
        }
    });`
          ),
        ],
      },
      {
        id: "transaction",
        title: "The SDK owns PKCE and state",
        blocks: [
          paragraph(
            "Begin creates an in-memory transaction with a verifier, state, redirect URL, and five-minute expiry. Complete identifies that transaction, verifies the callback state, and consumes it once. This prevents one callback from being replayed into another attempt."
          ),
          bullets(
            "AuthorizationUrl is safe to open, but do not log its complete query string.",
            "TransactionId is an opaque lookup key, not an auth token.",
            "The PKCE verifier stays internal and is never persisted.",
            "A missing, expired, mismatched, or already-consumed transaction fails before token exchange.",
            "CreateData is sent only when PocketBase needs to create the auth record."
          ),
        ],
      },
      {
        id: "assisted",
        title: "Assisted OAuth needs two explicit gates",
        blocks: [
          paragraph(
            "Set bEnableAssistedOAuth in client configuration, then require the platform OAuthCallback capability to report Supported. The default Mac bridge currently reports RequiresConfiguration, so an ordinary development run cannot open a browser through the assisted node."
          ),
          callout(
            "warning",
            "Do not treat editor browser launch as provider proof",
            "A complete packaged test must prove the URL scheme, provider redirect, app activation, realtime handoff, state validation, and code exchange together."
          ),
          paragraph(
            "Pinned integration automation never enables assisted OAuth and never opens a browser. Keep browser flows out of unattended test runs unless a dedicated harness owns every external step."
          ),
        ],
      },
    ],
  },
  {
    slug: "authentication/account",
    title: "Account actions",
    eyebrow: "Authentication",
    description: "Handle reset, verification, email change, and linked external auth operations without leaking short-lived secrets.",
    readTime: "9 min",
    sections: [
      {
        id: "actions",
        title: "Available auth collection actions",
        blocks: [
          table(
            ["Action", "Inputs", "Local session effect"],
            [
              ["Request Password Reset", "Email", "None"],
              ["Confirm Password Reset", "Token, New Password, Confirm Password", "None"],
              ["Request Email Verification", "Email", "None"],
              ["Confirm Email Verification", "Token", "Updates matching current auth record and persisted session"],
              ["Request Email Change", "New Email", "None"],
              ["Confirm Email Change", "Token, Current Password", "Clears matching local session"],
              ["List Linked External Auths", "Record ID", "None"],
              ["Unlink External Auth", "Record ID, Provider", "None"]
            ]
          ),
          paragraph(
            "All actions start from an auth collection value and use the same request options, cancellation, and error shape as record operations. Focused Blueprint nodes keep token and password labels specific to the step."
          ),
        ],
      },
      {
        id: "reset",
        title: "Password reset",
        blocks: [
          steps(
            { title: "Request", text: "Send the email to Request Password Reset. PocketBase owns delivery and the configured email template." },
            { title: "Receive token", text: "Your reset page or app link obtains the token from the route you configured with PocketBase." },
            { title: "Confirm", text: "Send Token, New Password, and Confirm Password to Confirm Password Reset." },
            { title: "Sign in again", text: "After confirmation, return the user to a fresh login flow." }
          ),
          screenshot("[put a screenshot of a password reset UI calling Confirm Password Reset here]"),
        ],
      },
      {
        id: "verification-change",
        title: "Verification and email change",
        blocks: [
          paragraph(
            "Confirming verification updates a matching current auth record and secure session before the action completes. Confirming an email change clears the matching session because the previous identity should not remain silently active. Refresh Session when the server must be authoritative for all current-record fields."
          ),
          callout("danger", "Tokens are credentials", "Reset, verification, and email-change tokens belong only in the live completion flow. Do not store them in Blueprint defaults, assets, saves, command lines, ordinary config, analytics, or logs."),
        ],
      },
      {
        id: "external-auths",
        title: "Linked providers",
        blocks: [
          paragraph(
            "List Linked External Auths returns provider links for one auth record. Unlink External Auth removes a selected provider. Before unlinking, make sure the account has another usable sign-in method so the player cannot lock themselves out."
          ),
          screenshot("[put a screenshot of List Linked External Auths filling an account settings provider list here]"),
        ],
      },
    ],
  },
  {
    slug: "authentication/session",
    title: "Session state and refresh coordination",
    eyebrow: "Authentication",
    description: "Treat the client as the session owner, react to ordered changes, and understand exactly when reads can refresh or replay.",
    readTime: "13 min",
    sections: [
      {
        id: "snapshot",
        title: "Session snapshots do not expose the token",
        blocks: [
          table(
            ["Field", "Meaning"],
            [
              ["bAuthenticated", "Whether the client currently owns an auth session"],
              ["AuthCollection", "The auth collection that issued the session"],
              ["AuthGeneration", "Monotonic version used to reject stale async commits"],
              ["PersistenceState", "MemoryOnly, Persisted, Unavailable, or Failed"],
              ["Reason", "LoggedIn, LoggedOut, Refreshed, Restored, UserSwitched, or RecordUpdated"],
              ["AuthRecord", "Current typed auth record without the token"]
            ]
          ),
          paragraph(
            "Get Current Session and Get Current Auth Record expose direct success and failure execution paths in Blueprint. You do not need a separate Branch. Session Changed events arrive on the game thread in generation order."
          ),
          screenshot("[put a screenshot of Get Current Session using its direct success and failure paths here]"),
        ],
      },
      {
        id: "refresh",
        title: "Refresh is coordinated",
        blocks: [
          paragraph(
            "Refresh Session posts to the current auth collection's auth-refresh endpoint. Concurrent callers share one request. The response commits only when the auth generation captured at start is still current, so a late refresh cannot undo logout or replace a newer user."
          ),
          code(
            "cpp",
            `FDelegateHandle SessionHandle = Client->OnSessionChanged().AddLambda(
    [](const FOpenPocketBaseSessionSnapshot& Session)
    {
        ApplySignedInUi(Session.bAuthenticated, Session.AuthRecord);
    });

Client->RefreshAuth(OnRefreshComplete);`
          ),
          callout("note", "PocketBase stays authoritative", "JWT exp decoding is only a scheduling hint. The SDK does not locally accept or reject a token based on an unverified payload."),
        ],
      },
      {
        id: "automatic",
        title: "Automatic refresh and replay rules",
        blocks: [
          bullets(
            "A valid numeric exp claim inside AuthRefreshLeadTimeSeconds may trigger proactive refresh before a request.",
            "Missing, malformed, or non-numeric expiry data simply disables that hint.",
            "Concurrent authenticated operations wait behind the same refresh.",
            "An eligible read rejected for authentication may refresh and replay once when policy allows it.",
            "Create, update, delete, batch, upload, and download never replay automatically after a response.",
            "The refresh response and generation check remain the final authority."
          ),
        ],
      },
      {
        id: "record-sync",
        title: "Current-user mutations synchronize the session",
        blocks: [
          paragraph(
            "A successful Update Record or batch result for the current auth record updates memory, secure persistence, auth generation, and the RecordUpdated event as one commit. If secure storage fails, the old record and generation remain. Deleting the current auth record clears the session and publishes LoggedOut."
          ),
          callout("success", "Old refreshes cannot win", "Every current-user record change increments the generation, so an older in-flight refresh cannot replace the new record after its response arrives."),
        ],
      },
      {
        id: "logout",
        title: "Logout is an immediate local clear",
        blocks: [
          paragraph(
            "PocketBase regular auth tokens are stateless and there is no server logout endpoint. Logout clears the token, current record, auth collection, and persisted session immediately, then emits LoggedOut."
          ),
          screenshot("[put a screenshot of Logout followed by Session Changed updating the main menu here]"),
        ],
      },
    ],
  },
  {
    slug: "authentication/persistence",
    title: "Secure session persistence",
    eyebrow: "Authentication",
    description: "Opt in to device secure storage, restore with or without server verification, and understand every restore status.",
    readTime: "11 min",
    sections: [
      {
        id: "opt-in",
        title: "Memory only is the default",
        blocks: [
          paragraph(
            "Set SessionPersistence to RequireSecureStorage before client creation only when the app should keep login across launches. Creation fails if the current platform has no supported secure store. There is no fallback to SaveGame, .ini, plain JSON, platform stored values, or editor preferences."
          ),
          code(
            "cpp",
            `FOpenPocketBaseClientConfig Config;
Config.BaseUrl = TEXT("https://pb.example.com");
Config.ProfileName = TEXT("production");
Config.SessionPersistence =
    EOpenPocketBaseSessionPersistence::RequireSecureStorage;

FOpenPocketBaseClientResult Created = FOpenPocketBaseClient::Create(Config);`
          ),
          callout("danger", "Fail closed", "If secure persistence is required but unavailable, the client is not created. The SDK never turns that policy into plaintext persistence."),
        ],
      },
      {
        id: "envelope",
        title: "What is persisted",
        blocks: [
          paragraph(
            "The size-bounded envelope is bound to the normalized PocketBase origin and profile. It stores the auth collection, token, authenticated record JSON, and schema metadata. Invalid or expired state is deleted without logging its contents."
          ),
          bullets(
            "Passwords are never persisted.",
            "OAuth codes and PKCE verifiers are never persisted.",
            "OTP and MFA values are never persisted.",
            "Protected-file tokens are never persisted.",
            "A session for one origin or profile cannot be restored into another."
          ),
        ],
      },
      {
        id: "restore",
        title: "Restore locally or verify with PocketBase",
        blocks: [
          code(
            "cpp",
            `Client->RestoreSession(
    true,
    [](TOpenPocketBaseResult<FOpenPocketBaseSessionRestoreResult>&& Result)
    {
        if (!Result.IsSuccess())
        {
            ShowSecureStoreIoError(Result.GetError());
            return;
        }

        HandleRestoreStatus(Result.GetValue().Status);
    });`
          ),
          table(
            ["Status", "Meaning"],
            [
              ["Restored", "A valid local envelope was loaded without a server check."],
              ["Verified", "The envelope was loaded and Auth Refresh verified it with PocketBase."],
              ["NotFound", "No saved session exists."],
              ["Expired", "Server verification rejected or expired the restored session."],
              ["Corrupt", "The envelope failed validation or decoding and was removed."],
              ["Unavailable", "The secure store cannot be used in this environment."],
              ["PolicyRejected", "Origin, profile, schema, or another persistence policy rejected the envelope."]
            ]
          ),
          screenshot("[put a screenshot of Restore Session branching on the restore status enum here]"),
        ],
      },
      {
        id: "platforms",
        title: "Platform status",
        blocks: [
          bullets(
            "Apple Keychain is implemented for Mac and iOS.",
            "Packaged Mac ARM64 has completed the secure persistence probe.",
            "iOS still needs packaged-target validation.",
            "Android Keystore is not implemented.",
            "Windows Credential Locker is not implemented.",
            "Linux secret service integration is not implemented."
          ),
          callout("note", "Gate the UI with capabilities", "Check SecurePersistence before showing a remember-me option. Explain an unavailable platform honestly instead of letting client creation fail after the user opts in."),
        ],
      },
    ],
  },
];
