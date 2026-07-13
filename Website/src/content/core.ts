import { bullets, callout, code, paragraph, screenshot, steps, table, type DocPage } from "./types";

export const corePages: DocPage[] = [
  {
    slug: "core/configuration",
    title: "Configuration and profiles",
    eyebrow: "Core SDK",
    description: "Keep origins and safe policy in project settings, keep secrets at runtime, and understand every client option.",
    readTime: "9 min",
    sections: [
      {
        id: "profiles",
        title: "Use profiles for non-secret settings",
        blocks: [
          paragraph(
            "Open Project Settings > Plugins > Open PocketBase SDK to define named profiles. A profile is a source-controlled bundle of connection policy. It can hold an origin, locale, session persistence policy, auth refresh behavior, and the assisted OAuth gate. It cannot hold credentials or arbitrary headers."
          ),
          screenshot("[put a screenshot of the Open PocketBase SDK profile settings panel here]"),
          code(
            "cpp",
            `const UOpenPocketBaseProjectSettings* Settings =
    GetDefault<UOpenPocketBaseProjectSettings>();

FOpenPocketBaseClientConfig Config;
FOpenPocketBaseError Error;
if (!Settings->TryResolveProfile(TEXT("production"), Config, Error))
{
    PresentConfigurationError(Error);
    return;
}`
          ),
          callout(
            "danger",
            "Project settings are not a secret store",
            "Never put passwords, auth tokens, OAuth codes, API keys, protected-file tokens, or reset tokens in an asset, .ini file, source default, command line, or Blueprint default."
          ),
        ],
      },
      {
        id: "config-fields",
        title: "Client config, field by field",
        blocks: [
          table(
            ["Field", "Default", "Purpose"],
            [
              ["BaseUrl", "required", "PocketBase origin only, such as https://pb.example.com. No credentials, path, query, or fragment."],
              ["ProfileName", "empty", "Separates persisted session envelopes and records which named policy created the client."],
              ["AcceptLanguage", "empty", "Sends PocketBase's supported locale hint without opening arbitrary headers."],
              ["DefaultHeaders", "empty", "Native-only bounded project headers. Protected and framing headers are rejected."],
              ["SessionPersistence", "MemoryOnly", "Choose RequireSecureStorage only when creation should fail without a supported secure store."],
              ["bProactiveAuthRefresh", "true", "Uses a valid numeric JWT exp field as a refresh scheduling hint."],
              ["AuthRefreshLeadTimeSeconds", "30", "Starts proactive refresh when the hinted expiry is this close."],
              ["bRetryEligibleReadsAfterAuthRefresh", "true", "Allows one refresh and replay after an eligible read receives an auth rejection."],
              ["bEnableAssistedOAuth", "false", "Opt-in gate for the browser and realtime assisted OAuth flow."]
            ]
          ),
          callout(
            "note",
            "BaseUrl is an origin",
            "Use https://pb.example.com, not https://pb.example.com/api, not a collection URL, and not a URL containing a username or password. Operations add their own paths."
          ),
        ],
      },
      {
        id: "development-override",
        title: "Local development override",
        blocks: [
          paragraph(
            "For local automation, enable the development override in project settings and set OPENPOCKETBASE_DEVELOPMENT_BASE_URL to an HTTP or HTTPS origin. Shipping builds ignore it. Validation rejects credentials, paths, queries, and fragments."
          ),
          paragraph("The project setting is labeled Allow OPENPOCKETBASE_DEVELOPMENT_BASE_URL override. Leave it disabled for any workflow that does not need a local process override."),
          code("powershell", `$env:OPENPOCKETBASE_DEVELOPMENT_BASE_URL = "http://127.0.0.1:8090"`),
          bullets(
            "Use it to point editor and test runs at a disposable local server.",
            "Do not use it as a general configuration channel.",
            "Do not put a token, password, or key in the value."
          ),
        ],
      },
    ],
  },
  {
    slug: "core/client-lifecycle",
    title: "Client ownership and lifecycle",
    eyebrow: "Core SDK",
    description: "Create one clear owner, retrieve clients safely, use named instances only when needed, and shut work down deterministically.",
    readTime: "7 min",
    sections: [
      {
        id: "default-client",
        title: "The default Game Instance client",
        blocks: [
          paragraph(
            "Initialize PocketBase creates the default client in the Game Instance subsystem. That is the best fit for most games because auth and backend access normally span maps. Get PocketBase Client returns the same wrapper anywhere with a valid world."
          ),
          steps(
            { title: "Initialize once", text: "Call Initialize PocketBase from Game Instance Init or one equivalent startup owner." },
            { title: "Retrieve elsewhere", text: "Call the pure Get PocketBase Client node. No client name is needed for the default instance." },
            { title: "Shut down deliberately", text: "Call Shutdown PocketBase if your application needs to stop requests before Game Instance teardown." }
          ),
          screenshot("[put a screenshot of Game Instance Init creating the default PocketBase client here]"),
        ],
      },
      {
        id: "named-clients",
        title: "Named clients are an advanced tool",
        blocks: [
          paragraph(
            "Create Named PocketBase Client only when the application truly needs separate PocketBase origins or separate authentication stores. A region selector, staging console, or operator tool may justify that. Different collections on the same backend do not."
          ),
          table(
            ["Need", "Use"],
            [
              ["Many collections on one backend", "One default client and many Collection values"],
              ["One backend, one signed-in player", "One default client"],
              ["Production and staging at the same time", "Two named clients"],
              ["User client plus trusted admin process", "Core client plus separate admin client in a trusted target"]
            ]
          ),
        ],
      },
      {
        id: "shutdown",
        title: "What shutdown guarantees",
        blocks: [
          bullets(
            "Active requests are cancelled exactly once.",
            "Realtime subscriptions are stopped.",
            "Late transport callbacks are ignored.",
            "No new work is accepted after shutdown.",
            "Game Instance teardown shuts down every remaining default and named client."
          ),
          code("cpp", `void UMyGameInstance::Shutdown()
{
    if (PocketBase.IsValid())
    {
        PocketBase->Shutdown();
        PocketBase.Reset();
    }

    Super::Shutdown();
}`),
          callout(
            "warning",
            "A request handle is not its owner",
            "Destroying a handle does not cancel the request. The client owns active work until completion, explicit cancellation, replacement by request key, or shutdown."
          ),
        ],
      },
    ],
  },
  {
    slug: "core/requests-errors",
    title: "Requests, cancellation, retries, and errors",
    eyebrow: "Core SDK",
    description: "Tune request policy when needed and build one honest error path for every SDK feature.",
    readTime: "12 min",
    sections: [
      {
        id: "terminal-contract",
        title: "One request, one terminal result",
        blocks: [
          paragraph(
            "Every public request completes once with success, failure, or cancellation. Completion and progress are dispatched on the game thread. Cancellation is explicit, and a dropped handle does not stop work. This makes it safe to centralize loading state around terminal outputs."
          ),
          screenshot("[put a screenshot of Success, Failed, and Cancelled outputs feeding one UI completion path here]"),
          callout("note", "Progress is not completion", "Upload and download progress can be coalesced. Never infer success from reaching a byte count. Wait for the terminal success output."),
        ],
      },
      {
        id: "options",
        title: "Request options",
        blocks: [
          table(
            ["Option", "Default", "How to use it"],
            [
              ["TotalTimeoutSeconds", "30", "Absolute wall-clock ceiling for all attempts and delays."],
              ["ActivityTimeoutSeconds", "15", "Fails when the active transfer makes no progress for this long."],
              ["RequestKey", "empty", "Names a logical request, useful for screen refreshes and search boxes."],
              ["bCancelPreviousRequestWithSameKey", "false", "Cancels older active work with the same key before starting this request."],
              ["AdditionalHeaders", "empty", "Bounded project headers. Authorization, cookies, host, framing, content type, request IDs, and Sec-* cannot be replaced."],
              ["TraceParent", "empty", "A separately validated W3C traceparent value."],
              ["bRetryEligibleReads", "true", "Allows retries for requests whose semantics remain safe."],
              ["MaxReadRetries", "2", "Maximum retry count after the first read attempt."],
              ["RetryBaseDelaySeconds", "0.25", "Initial exponential backoff delay."],
              ["RetryMaxDelaySeconds", "2", "Upper bound for one retry delay."],
              ["RetryJitterFraction", "0.2", "Randomizes delays to avoid synchronized retry waves."],
              ["MaxResponseBytes", "8 MiB", "Rejects oversized responses before unbounded memory use."]
            ]
          ),
          paragraph(
            "Create, update, delete, batch, upload, and download are never replayed after an ambiguous response. A retry could duplicate a side effect or hide an unknown transaction outcome. Ordinary eligible reads may retry, and an auth-rejected read may refresh and replay once when client policy allows it."
          ),
        ],
      },
      {
        id: "error-shape",
        title: "Read the error in layers",
        blocks: [
          table(
            ["Field", "Meaning"],
            [
              ["Kind", "Stable client category: Cancelled, InvalidArgument, Transport, Timeout, Http, PocketBase, Serialization, Authentication, SecureStorage, OfflineQueue, Unsupported, or Internal."],
              ["HttpStatus", "Sanitized response status when a response exists. Zero when there is no HTTP response."],
              ["ServerCode", "PocketBase or SDK server-style code, useful for branching on known conditions."],
              ["ServerMessage", "Sanitized human-facing detail. Do not treat it as a stable program key."],
              ["FieldErrors", "Per-field validation codes and messages from PocketBase."],
              ["bMayRetry", "A hint for caller-owned retry UI, not permission to replay mutations automatically."],
              ["RequestId", "A safe correlation value when one is available."]
            ]
          ),
          code(
            "cpp",
            `void ShowPocketBaseError(const FOpenPocketBaseError& Error)
{
    if (Error.Kind == EOpenPocketBaseErrorKind::Cancelled)
    {
        return;
    }

    if (const FOpenPocketBaseFieldError* Title = Error.FieldErrors.Find(TEXT("title")))
    {
        SetTitleError(Title->Message);
    }
    else
    {
        SetGeneralError(Error.ServerMessage);
    }
}`
          ),
        ],
      },
      {
        id: "request-keys",
        title: "Replace stale screen requests",
        blocks: [
          paragraph(
            "Request keys are useful when a newer query makes an older one irrelevant. A live search box can set RequestKey to task-search and enable cancellation of the previous request. The cancelled request still gets its Cancelled terminal output, so its owner can clean up."
          ),
          callout(
            "warning",
            "Do not reuse a key for unrelated work",
            "A broad key such as api can make one screen cancel another screen's request. Scope keys to the client and user action, such as inventory-refresh or friends-search."
          ),
        ],
      },
    ],
  },
  {
    slug: "core/capabilities",
    title: "Capabilities and platform gates",
    eyebrow: "Core SDK",
    description: "Ask the client what a packaged platform can actually do and fail honestly when a bridge has not been proven.",
    readTime: "6 min",
    sections: [
      {
        id: "why",
        title: "Editor success is not packaged proof",
        blocks: [
          paragraph(
            "HTTP streaming, secure stores, callback handling, and long-lived SSE can behave differently in a packaged target. The SDK reports a structured capability instead of assuming that an editor test proves a device integration."
          ),
          table(
            ["Capability", "Covers"],
            [
              ["HttpStreaming", "Incremental uploads, downloads, and response streaming"],
              ["SecurePersistence", "A platform secure store suitable for auth sessions"],
              ["OAuthCallback", "A configured app-to-browser-to-app provider flow"],
              ["OfflineModule", "The optional offline outbox module"],
              ["EditorMock", "Editor-only scripted transport support"],
              ["PrivilegedModule", "Availability of trusted admin tooling"]
            ]
          ),
        ],
      },
      {
        id: "status",
        title: "Capability status is actionable",
        blocks: [
          table(
            ["Status", "Meaning"],
            [
              ["Supported", "Implemented and allowed by the current build and policy."],
              ["Unavailable", "The implementation exists but cannot be used in the current environment."],
              ["DisabledByPolicy", "The caller did not enable an explicit policy gate."],
              ["RequiresConfiguration", "Platform or provider configuration is still needed."],
              ["Restricted", "The build or target intentionally prevents the feature."],
              ["TemporarilyUnavailable", "The capability may recover later."],
              ["Unsupported", "No supported implementation is available for this platform or build."]
            ]
          ),
          code(
            "cpp",
            `const FOpenPocketBaseCapabilityInfo SecureStore =
    Client->GetCapability(EOpenPocketBaseCapability::SecurePersistence);

if (SecureStore.Status != EOpenPocketBaseCapabilityStatus::Supported)
{
    ShowSessionPersistenceExplanation(SecureStore.Reason);
}`
          ),
          screenshot("[put a screenshot of a Blueprint capability report driving a platform support panel here]"),
        ],
      },
      {
        id: "current-proof",
        title: "Current packaged proof",
        blocks: [
          bullets(
            "Core HTTP records and authentication are implemented across the runtime module.",
            "Mac ARM64 Development packaging has proven Apple Keychain, trusted HTTPS, multipart upload, protected download, and incremental SSE reconnect behavior.",
            "iOS Keychain code exists but still needs packaged-target validation.",
            "Android, Windows, and Linux secure stores are not implemented and do not fall back to plaintext.",
            "The default Mac assisted OAuth bridge reports RequiresConfiguration until a full provider callback flow is packaged and verified."
          ),
          callout("success", "Failure is explicit", "A gated feature returns Unsupported or another capability status. It does not pretend to work and fail later with an unrelated transport message."),
        ],
      },
    ],
  },
];
