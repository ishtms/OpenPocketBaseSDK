import { bullets, callout, code, paragraph, screenshot, steps, table, type DocPage } from "./types";

export const operationPages: DocPage[] = [
  {
    slug: "files/uploads",
    title: "Upload files with record mutations",
    eyebrow: "Files",
    description: "Choose disk or memory inputs, apply field modifiers, set hard limits, and report game-thread progress without buffering large files.",
    readTime: "11 min",
    sections: [
      {
        id: "record-mutation",
        title: "An upload is a record create or update",
        blocks: [
          paragraph(
            "PocketBase file fields belong to records. Use Create Record with Files or Update Record with Files, not a standalone upload endpoint. The multipart body combines ordinary record fields with one or more FOpenPocketBaseFileInput values."
          ),
          table(
            ["Input source", "Use when", "Memory behavior"],
            [
              ["FilePath", "A file already exists on disk, especially a large capture or package", "Read incrementally as Unreal HTTP consumes the multipart stream"],
              ["Bytes", "Small generated content already exists in memory", "Retained as a bounded inline byte array"]
            ]
          ),
          callout("note", "File fields use the same modifiers", "Replace, Append, Prepend, and Remove map to PocketBase's file-field modifier naming. Choose one the collection field actually supports."),
        ],
      },
      {
        id: "input",
        title: "Describe every file explicitly",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseFileInput DiskFile;
DiskFile.FieldName = TEXT("attachments");
DiskFile.FileName = TEXT("report.pdf");
DiskFile.ContentType = TEXT("application/pdf");
DiskFile.FilePath = ReportPath;

FOpenPocketBaseFileInput GeneratedFile;
GeneratedFile.FieldName = TEXT("attachments");
GeneratedFile.FileName = TEXT("summary.txt");
GeneratedFile.ContentType = TEXT("text/plain");
GeneratedFile.Modifier = EOpenPocketBaseFieldModifier::Append;
GeneratedFile.bUseFilePath = false;
GeneratedFile.Bytes = SummaryBytes;`
          ),
          bullets(
            "FieldName is the PocketBase file field, not a local variable name.",
            "FileName is the safe name sent in multipart metadata.",
            "ContentType should describe the actual bytes and defaults to application/octet-stream.",
            "bUseFilePath chooses exactly one source. Do not populate both and expect fallback behavior.",
            "Paths, filenames, MIME types, modifiers, and inline sizes are validated before dispatch."
          ),
        ],
      },
      {
        id: "limits",
        title: "Put limits at the caller boundary",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseUploadLimits Limits;
Limits.MaxFiles = 8;
Limits.MaxInlineFileBytes = 1024 * 1024;
Limits.MaxSourceFileBytes = 100 * 1024 * 1024;
Limits.MaxTotalBodyBytes = 128 * 1024 * 1024;`
          ),
          table(
            ["Limit", "Protects"],
            [
              ["MaxFiles", "Unexpectedly large multipart part counts"],
              ["MaxInlineFileBytes", "One in-memory input"],
              ["MaxSourceFileBytes", "One disk input before streaming starts"],
              ["MaxTotalBodyBytes", "Complete multipart body including fields and framing"]
            ]
          ),
          paragraph("These are local upper bounds, not replacements for PocketBase collection file limits, reverse proxy limits, or storage policy. The smallest applicable limit wins."),
        ],
      },
      {
        id: "progress",
        title: "Progress is coalesced and ordered",
        blocks: [
          paragraph(
            "Transfer progress carries 64-bit TransferredBytes, optional TotalBytes, attempt number, and phase. Updates are coalesced after 64 KiB or 100 ms, dispatched on the game thread, and never emitted after success, failure, or cancellation."
          ),
          screenshot("[put a screenshot of Create Record with Files Progress updating a UMG progress bar here]"),
          callout("warning", "No automatic mutation retry", "An interrupted upload may have reached PocketBase even when the response did not return. The SDK does not replay it automatically."),
        ],
      },
    ],
  },
  {
    slug: "files/downloads",
    title: "File URLs and protected downloads",
    eyebrow: "Files",
    description: "Build public URLs without credentials, obtain short-lived protected tokens, and stream bounded downloads safely to memory or disk.",
    readTime: "13 min",
    sections: [
      {
        id: "public-url",
        title: "Build a public file URL",
        blocks: [
          paragraph(
            "Try Build File URL encodes collection, record ID, filename, thumbnail options, crop mode, and forced-download policy. It never accepts a protected token, so the returned string is safe to keep for public files without retaining a credential."
          ),
          table(
            ["Thumbnail mode", "Behavior"],
            [
              ["None", "No thumbnail parameters"],
              ["CropCenter", "Fill dimensions and crop around the center"],
              ["CropTop", "Fill dimensions while favoring the top"],
              ["CropBottom", "Fill dimensions while favoring the bottom"],
              ["Fit", "Fit inside dimensions without cropping"]
            ]
          ),
          screenshot("[put a screenshot of Try Build File URL feeding an Unreal image loading workflow here]"),
        ],
      },
      {
        id: "protected",
        title: "Protected files use an opaque token",
        blocks: [
          steps(
            { title: "Request late", text: "Call Get Protected File Token immediately before the eligible download." },
            { title: "Keep it opaque", text: "Do not convert the token to a string, store it, log it, or add it to a public URL yourself." },
            { title: "Pass directly", text: "Connect the token success value to Download File's advanced Token input." },
            { title: "Discard", text: "Let the short-lived value leave scope after the request begins." }
          ),
          screenshot("[put a screenshot of Get Protected File Token connected directly to Download File here]"),
          callout("danger", "A protected token is a credential", "Treat it like a short-lived password for file access. The SDK sanitizes download errors so a transport cannot echo the token-bearing URL."),
        ],
      },
      {
        id: "targets",
        title: "Choose memory or file output",
        blocks: [
          table(
            ["Target", "Result", "Use"],
            [
              ["Memory", "Bytes in FOpenPocketBaseFileDownloadResult", "Small data needed immediately by code"],
              ["File", "bSavedToFile and DestinationPath", "Large assets, exports, patches, or later file processing"]
            ]
          ),
          code(
            "cpp",
            `FOpenPocketBaseFileDownloadOptions Options;
Options.Target = EOpenPocketBaseFileDownloadTarget::File;
Options.DestinationPath = DestinationPath;
Options.bReplaceExisting = false;
Options.MaxBytes = 128 * 1024 * 1024;

Client->Files().Download(
    TEXT("tasks"),
    TEXT("task00000000001"),
    TEXT("report_ab12cd34.pdf"),
    MoveTemp(Options),
    OnDownloaded,
    MoveTemp(Token),
    OnProgress);`
          ),
        ],
      },
      {
        id: "disk-safety",
        title: "Disk downloads publish only complete files",
        blocks: [
          bullets(
            "The SDK writes to DestinationPath plus .tmp.",
            "An existing temporary owner is rejected instead of overwritten.",
            "MaxBytes is enforced while streaming.",
            "The archive closes on every terminal path.",
            "Success renames the complete temporary file into place.",
            "Failure or cancellation removes the temporary file.",
            "bReplaceExisting must be explicit before a final destination can be replaced."
          ),
          paragraph(
            "The result also carries sanitized status, content type, actual length, filename hint, ETag, Last-Modified, and destination metadata. It never includes the protected URL or token."
          ),
        ],
      },
    ],
  },
  {
    slug: "realtime/subscriptions",
    title: "Realtime subscriptions",
    eyebrow: "Realtime",
    description: "Own subscriptions explicitly, listen to typed record events, handle reconnect gaps, and stop cleanly.",
    readTime: "12 min",
    sections: [
      {
        id: "subscribe",
        title: "Subscribe from a collection",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseRealtimeOptions Options;
Options.Filter = FOpenPocketBaseFilter::Boolean(
    TEXT("done"),
    EOpenPocketBaseBooleanComparison::Equals,
    false);

FOpenPocketBaseRealtimeCallbacks Callbacks;
Callbacks.OnEvent = [](const FOpenPocketBaseRealtimeEvent& Event)
{
    ApplyTaskEvent(Event.Action, Event.Record);
};

FOpenPocketBaseSubscriptionResult Result = Client
    ->Collection(TEXT("tasks"))
    .SubscribeToRecords(MoveTemp(Callbacks), MoveTemp(Options));`
          ),
          paragraph(
            "Blueprint offers Subscribe to Records for an entire collection and Subscribe to Record for one ID. Subscribe to Realtime Topic remains in the advanced category for custom PocketBase topics. Every subscribe call either returns one owned subscription or a structured error."
          ),
          screenshot("[put a screenshot of Collection(tasks) -> Subscribe to Records with a typed filter here]"),
        ],
      },
      {
        id: "events",
        title: "Read typed events first",
        blocks: [
          table(
            ["Field", "Meaning"],
            [
              ["Topic", "The registered PocketBase topic"],
              ["Action", "Unknown, Create, Update, or Delete"],
              ["ActionName", "Original action text for forward compatibility"],
              ["bHasRecord", "Whether a typed record was parsed"],
              ["Record", "Typed record metadata, Data, and Expanded values"],
              ["Data", "Additional custom payload fields only"]
            ]
          ),
          paragraph("Keep collection fields in Event.Record.Data. Event.Data is reserved for additional custom payload values and does not duplicate the record."),
        ],
      },
      {
        id: "ownership",
        title: "The subscription is the lifetime handle",
        blocks: [
          bullets(
            "Store the Open Pocket Base Subscription in the system that consumes updates.",
            "Bind Event, Connection State, Error, and Resync Required delegates.",
            "Call Unsubscribe when the owner leaves the screen, world, or feature.",
            "Use Unsubscribe All Realtime only when the client owner is intentionally stopping all live work.",
            "Client shutdown stops every remaining subscription."
          ),
          screenshot("[put a screenshot of a stored subscription binding all four realtime delegates here]"),
        ],
      },
      {
        id: "reconnect",
        title: "Reconnect does not hide gaps",
        blocks: [
          paragraph(
            "Connection state moves through Created, Subscribing, Active, Reconnecting, and Stopped. After a network loss the manager starts a fresh connection generation and re-registers exact topics. Realtime does not claim durable delivery, so a gap emits Resync Required."
          ),
          callout("warning", "Resync from the record API", "When Resync Required fires, fetch authoritative state with a bounded list or a record read. Do not assume the next realtime event repairs everything missed during the gap."),
          paragraph("Incremental SSE is capability-gated per packaged platform. An unproven target reports Unsupported instead of returning a subscription that cannot maintain a reliable connection."),
        ],
      },
    ],
  },
  {
    slug: "tools/custom-routes",
    title: "Health checks and custom routes",
    eyebrow: "Tools",
    description: "Call project-defined PocketBase routes while preserving origin, auth, bounds, redirects, errors, cancellation, and response parsing.",
    readTime: "11 min",
    sections: [
      {
        id: "health",
        title: "Check the pinned server",
        blocks: [
          paragraph(
            "Check Health calls PocketBase's public /api/health route. The result includes bHealthy, HTTP status, PocketBase code and message, and elapsed seconds through the normal cancellable request lifecycle."
          ),
          code("cpp", `Client->Health([](TOpenPocketBaseResult<FOpenPocketBaseHealthResult>&& Result)
{
    if (Result.IsSuccess() && Result.GetValue().bHealthy)
    {
        ShowBackendOnline(Result.GetValue().DurationSeconds);
    }
});`),
          screenshot("[put a screenshot of Check Health driving an online status badge here]"),
        ],
      },
      {
        id: "request",
        title: "Describe one explicit request body",
        blocks: [
          table(
            ["Format", "Use"],
            [
              ["None", "GET or another route with no request body"],
              ["Json", "One JSON object wrapper"],
              ["Form", "URL-encoded string fields"],
              ["Multipart", "Generic streamed multipart fields and files"],
              ["Raw", "Text-like bytes with an explicit content type"],
              ["Binary", "Exact opaque bytes with an explicit content type"]
            ]
          ),
          code(
            "cpp",
            `FOpenPocketBaseCustomRouteRequest Request;
Request.Method = EOpenPocketBaseCustomRouteMethod::Post;
Request.Path = TEXT("/api/project/echo");
Request.Query.Add(TEXT("view"), TEXT("summary"));
Request.bUseAuth = true;
Request.BodyFormat = EOpenPocketBaseCustomBodyFormat::Json;
Request.JsonBody.JsonObject = MakeShared<FJsonObject>();
Request.JsonBody.JsonObject->SetStringField(TEXT("message"), TEXT("hello"));
Request.Options.TraceParent = TraceParent;

Client->SendCustomRoute(MoveTemp(Request), OnRouteComplete);`
          ),
          callout("note", "Stay on the configured origin", "Path is a project route such as /api/project/echo. It cannot redirect the client to a new origin or smuggle credentials through a URL."),
        ],
      },
      {
        id: "response",
        title: "Read JSON or exact bytes",
        blocks: [
          paragraph(
            "The response retains status, content type, request ID, exact body bytes, elapsed time, and optional parsed JSON. JsonRootType distinguishes Object, Array, and Scalar. Object roots are also exposed through JsonBody for Blueprint. Native GetParsedJson returns the retained JSON value for every root shape."
          ),
          bullets(
            "Set MaxResponseBytes for the route's expected upper bound.",
            "Use body bytes when the route is binary or its JSON shape is intentionally custom.",
            "Check bHasJson before reading JsonRootType or JsonBody.",
            "Use the shared error path for invalid routes, response bounds, parsing, timeouts, and cancellation."
          ),
        ],
      },
      {
        id: "headers",
        title: "Headers are constrained on purpose",
        blocks: [
          paragraph(
            "Additional headers cannot replace authorization, cookies, host, content framing, content type, API keys, request IDs, hop-by-hop headers, or Sec-* headers. TraceParent has its own validated field. There are no arbitrary request or response interceptors in Blueprint."
          ),
          callout("warning", "Use a server route for secrets", "A distributed game cannot safely keep a backend secret. Put privileged upstream calls behind your PocketBase project route and authorize the signed-in player there."),
        ],
      },
    ],
  },
  {
    slug: "tools/admin-api",
    title: "Privileged admin API",
    eyebrow: "Trusted tools",
    description: "Use the separate admin module in editor, operator, or server contexts with explicit policy, hard bounds, and no Shipping exposure by default.",
    readTime: "15 min",
    sections: [
      {
        id: "boundary",
        title: "This module belongs behind a trust boundary",
        blocks: [
          paragraph(
            "OpenPocketBaseSDKAdmin covers superuser routes for tooling, not gameplay. Add it only to a trusted editor module, operator app, or server-side target. The plugin descriptor excludes the complete module from Shipping, so ordinary Shipping binaries contain no admin symbols or reflection data."
          ),
          callout("danger", "Never ship superuser credentials", "Do not put a superuser email or password in a Blueprint asset, source default, command argument, save, or ordinary .ini file. A distributed client cannot keep it secret."),
        ],
      },
      {
        id: "policy",
        title: "Every dangerous class of work has a policy gate",
        blocks: [
          table(
            ["Policy", "Default", "Unlocks"],
            [
              ["bEnablePrivilegedRequests", "false", "Any admin request"],
              ["bAllowInShipping", "false", "Runtime policy half of an exceptional Shipping setup"],
              ["bAllowDestructiveCollectionImport", "false", "Destructive collection import"],
              ["bAllowBackupRestore", "false", "Backup restore"],
              ["bAllowSqlWrites", "false", "SQL beyond one conservative SELECT"],
              ["bAllowImpersonation", "false", "Isolated user impersonation"],
              ["MaxPageSize", "100", "Admin list page bound"],
              ["MaxRequestBytes", "8 MiB", "Admin request body bound"],
              ["MaxResponseBytes", "8 MiB", "Admin response body bound"],
              ["MaxBackupBytes", "64 MiB", "Backup upload and download bound"],
              ["MaxSqlRows", "1000", "Raw SQL result row bound"]
            ]
          ),
          code(
            "cpp",
            `FOpenPocketBaseAdminPolicy Policy;
Policy.bEnablePrivilegedRequests = true;
Policy.bAllowImpersonation = true;
Policy.MaxSqlRows = 250;

FOpenPocketBaseAdminClientResult Result =
    FOpenPocketBaseAdminClient::Create(Config, Policy);`
          ),
        ],
      },
      {
        id: "inventory",
        title: "Admin operation inventory",
        blocks: [
          table(
            ["Area", "Operations"],
            [
              ["Authentication", "Authenticate PocketBase Superuser, Logout"],
              ["Collections", "List, get, create, update, delete, and import"],
              ["Settings", "Get, update, test S3, and test email"],
              ["Logs", "List and get"],
              ["Backups", "List, create, upload, download, restore, and delete"],
              ["Crons", "List and run"],
              ["SQL", "Run bounded raw SQL"],
              ["Users", "Create an isolated impersonated core client"]
            ]
          ),
          screenshot("[put a screenshot of Initialize Privileged PocketBase -> Authenticate PocketBase Superuser -> List Collections here]"),
          paragraph(
            "Every Blueprint factory is marked Development Only. Admin actions retain their client and do not expose World Context Object pins after initialization. Responses use typed metadata plus JSON wrappers for version-dependent admin documents."
          ),
        ],
      },
      {
        id: "sql-impersonation",
        title: "SQL and impersonation have narrower contracts",
        blocks: [
          bullets(
            "Without bAllowSqlWrites, only one conservative SELECT is accepted.",
            "CTE writes, pragmas, and stacked statements are rejected locally.",
            "SQL results expose execution time, affected rows, row count, column metadata, and JSON rows.",
            "An impersonation result owns a separate memory-only core client.",
            "The impersonated client cannot persist or refresh its token.",
            "Settings responses recursively replace secret leaves with [REDACTED]."
          ),
          callout("warning", "Shipping requires three source-controlled decisions", "You must remove the descriptor deny entry, define OPENPOCKETBASESDK_ADMIN_SHIPPING_ENABLED=1 in the target, and set bAllowInShipping at runtime. Do not do this for a distributed game client."),
        ],
      },
    ],
  },
  {
    slug: "tools/security",
    title: "Security model",
    eyebrow: "Production",
    description: "Know which values can live in configuration, which must stay ephemeral, and which server-side rules still protect every request.",
    readTime: "10 min",
    sections: [
      {
        id: "client-trust",
        title: "A game client is not a trusted machine",
        blocks: [
          paragraph(
            "Players own the device, process, memory, and network path. Anything shipped in a build can be recovered. Use PocketBase API rules for per-record authorization and place service secrets or privileged upstream work behind trusted server code."
          ),
          callout("danger", "Never embed a superuser", "A hidden Blueprint variable, encrypted string, native constant, pak file, or remote config value is still recoverable from a distributed client."),
        ],
      },
      {
        id: "classification",
        title: "Classify values before storing them",
        blocks: [
          table(
            ["Value", "May persist in project config", "May persist in secure session", "Log"],
            [
              ["PocketBase origin", "Yes", "Envelope binding only", "Yes"],
              ["Profile name", "Yes", "Envelope binding only", "Yes"],
              ["Accept-Language", "Yes", "No", "Yes"],
              ["Regular auth token", "No", "Yes, when opted in", "No"],
              ["Password", "No", "No", "No"],
              ["OAuth code or PKCE verifier", "No", "No", "No"],
              ["OTP or MFA continuation", "No", "No", "No"],
              ["Protected-file token", "No", "No", "No"],
              ["Reset or verification token", "No", "No", "No"],
              ["Superuser credentials", "No", "No", "No"]
            ]
          ),
        ],
      },
      {
        id: "api-rules",
        title: "PocketBase rules remain authoritative",
        blocks: [
          bullets(
            "Use list and view rules to restrict record reads.",
            "Use create, update, and delete rules to enforce ownership and allowed transitions.",
            "Do not trust a client-provided owner field without a rule that ties it to the authenticated record.",
            "Protect file fields through the same collection view rules and protected-file configuration.",
            "Validate custom route authorization on the server even when bUseAuth sends a token."
          ),
          paragraph("The SDK validates shapes, bounds, origins, and lifecycle policy. It cannot replace business authorization on the backend."),
        ],
      },
      {
        id: "safe-observability",
        title: "Keep observability useful and sanitized",
        blocks: [
          paragraph(
            "Use Error.Kind, HttpStatus, ServerCode, RequestId, safe route names, durations, and aggregate sizes. Avoid full URLs with queries, arbitrary response bodies, headers, auth records with private fields, and every credential class listed above."
          ),
          callout("note", "Trace context has a dedicated field", "Use the validated TraceParent option for cross-service correlation. Do not work around protected-header rules with a differently cased header name."),
        ],
      },
    ],
  },
  {
    slug: "tools/platform-support",
    title: "Compatibility and platform support",
    eyebrow: "Production",
    description: "Match your PocketBase version, distinguish implementation from packaged proof, and plan the remaining device checks.",
    readTime: "8 min",
    sections: [
      {
        id: "versions",
        title: "Pinned versions",
        blocks: [
          table(
            ["Component", "Target"],
            [
              ["OpenPocketBase SDK", "0.1.0"],
              ["PocketBase server", "v0.39.11 at pinned commit 5d217ddb50cb144d80a5d0b0bdf11b52b2c3e457"],
              ["Reference JS vocabulary", "Official JavaScript SDK v0.28.0"],
              ["Packaged proof host", "Unreal Engine 5.8 Mac ARM64 Development"]
            ]
          ),
          paragraph(
            "The PocketBase server is authoritative. The JavaScript SDK helps align service vocabulary but does not override server behavior. Do not update the compatibility claim until the pinned migration and integration suite pass against the newer server."
          ),
        ],
      },
      {
        id: "matrix",
        title: "Current platform matrix",
        blocks: [
          table(
            ["Feature", "Mac ARM64", "iOS", "Android", "Windows", "Linux"],
            [
              ["Core HTTP API", "Implemented", "Implemented", "Implemented", "Implemented", "Implemented"],
              ["Secure session bridge", "Implemented and proven", "Implemented, unproven", "Not implemented", "Not implemented", "Not implemented"],
              ["Streaming transfer", "Packaged proven", "Needs proof", "Needs proof", "Needs proof", "Needs proof"],
              ["Incremental realtime SSE", "Packaged proven", "Needs proof", "Needs proof", "Needs proof", "Needs proof"],
              ["Assisted OAuth", "Requires configuration", "Needs implementation proof", "Needs implementation proof", "Needs implementation proof", "Needs implementation proof"]
            ]
          ),
          callout("note", "Unproven is not the same as broken", "It means the repository does not yet make a packaged support claim. Run the platform-neutral tests, then complete a target-specific probe before publishing support."),
        ],
      },
      {
        id: "mac-proof",
        title: "What the Mac package probe proves",
        blocks: [
          bullets(
            "An ephemeral Apple Keychain value round-trips and is deleted.",
            "A trusted HTTPS request completes.",
            "A 256 KiB disk file streams through multipart create.",
            "A protected-file token is obtained and the exact bytes stream back to disk.",
            "Final transfer progress and fixture cleanup complete.",
            "Authenticated realtime survives a forced network drop, starts a new generation, resubscribes exactly, signals the gap, and delivers again."
          ),
          paragraph("This does not prove OAuth browser handoff or any other platform. Each capability keeps its own evidence boundary."),
        ],
      },
    ],
  },
  {
    slug: "tools/testing",
    title: "Testing and local validation",
    eyebrow: "Production",
    description: "Use fast scripted contracts for request behavior, then run the pinned live server and packaged probes for boundaries mocks cannot prove.",
    readTime: "9 min",
    sections: [
      {
        id: "layers",
        title: "Test at the smallest useful boundary",
        blocks: [
          table(
            ["Layer", "Proves", "Does not prove"],
            [
              ["Editor automation with scripted transport", "Validation, URL construction, headers, serialization, retries, cancellation, state coordination", "PocketBase server behavior or packaged platform bridges"],
              ["Pinned integration server", "Actual v0.39.11 routes, fixtures, auth, CRUD, batch rollback, admin inventory", "Device keychain, packaged streaming, browser handoff"],
              ["Packaged target probe", "Platform secure store, HTTPS stack, incremental transfer, realtime lifecycle", "Other platforms or untested provider flows"]
            ]
          ),
          paragraph("A green narrow test is evidence only for the contract it exercises. Keep platform claims tied to a packaged target, not to editor success."),
        ],
      },
      {
        id: "pinned-server",
        title: "Pinned integration server",
        blocks: [
          paragraph(
            "Tests/Integration contains the server version, migration fixture, and runner. The fixture covers health, password login, record operations, transactional batches, rollback, and privileged route inventory."
          ),
          code("powershell", `cd Tests/Integration
./run_pinned_server.ps1`),
          callout("note", "Disposable superuser", "The runner creates a one-run superuser in a temporary data directory and removes it when the server exits, preventing the first-run setup browser without retaining credentials."),
        ],
      },
      {
        id: "packaging",
        title: "Packaged platform probe",
        blocks: [
          paragraph(
            "Tests/Packaging documents the isolated host and Mac TLS probe. Use the same pattern for a new target: keep the fixture disposable, verify exact bytes, force lifecycle failures where practical, and record only the capability actually proven."
          ),
          bullets(
            "Verify secure storage creates, reads, and deletes one ephemeral value.",
            "Verify trusted HTTPS with the packaged network stack.",
            "Verify multipart disk streaming and bounded disk download with exact bytes.",
            "Verify progress reaches its final state before completion.",
            "Verify realtime reconnection creates a fresh generation and signals a delivery gap.",
            "Verify temporary files and server fixtures are removed after the run."
          ),
        ],
      },
      {
        id: "test-seams",
        title: "Use the built-in test seams",
        blocks: [
          paragraph(
            "FOpenPocketBaseClientDependencies accepts an injected transport, secure store, clock, and OAuth browser. Small mocks can drive the public contract without adding production abstractions. Keep native device behavior in a targeted package probe when it cannot be automated locally."
          ),
        ],
      },
    ],
  },
];
