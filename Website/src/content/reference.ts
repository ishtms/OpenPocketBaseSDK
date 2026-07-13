import { bullets, callout, code, paragraph, table, type DocPage } from "./types";

export const referencePages: DocPage[] = [
  {
    slug: "reference/feature-status",
    title: "Feature status and non-goals",
    eyebrow: "Reference",
    description: "See what the current release implements, what remains platform-gated, and what is intentionally outside the first release.",
    readTime: "8 min",
    sections: [
      {
        id: "implemented",
        title: "Current surface",
        blocks: [
          table(
            ["Area", "Current status"],
            [
              ["Shared request lifecycle", "Implemented with validation, cancellation, timeouts, bounded retries, redaction, and game-thread completion"],
              ["Record CRUD and querying", "Implemented, including typed filters, projection, expand, truthful totals, and bounded full lists"],
              ["Authentication", "Password, OTP, MFA continuation, manual OAuth2, account actions, refresh coordination, and session events implemented"],
              ["Secure persistence", "Opt-in and fail-closed, packaged-proven on Mac ARM64"],
              ["Files", "Multipart mutations, URLs, tokens, memory and disk downloads implemented, packaged streaming proven on Mac ARM64"],
              ["Realtime", "Shared SSE connection, owned listeners, reconnect, and gap signaling implemented, packaged-proven on Mac ARM64"],
              ["Batches", "Explicit transactional create, update, upsert, and delete implemented"],
              ["Custom routes", "JSON, form, multipart, raw, and binary request formats implemented"],
              ["Privileged API", "Implemented in a separate Development Only module excluded from Shipping"],
              ["Offline module", "Reserved optional module contract. No public outbox API is exposed by the current source tree"]
            ]
          ),
        ],
      },
      {
        id: "deferred",
        title: "Explicitly deferred",
        blocks: [
          bullets(
            "Structured filter ASTs, relation path objects, sort builders, expand builders, and a custom Blueprint filter editor",
            "A first-class cursor abstraction over PocketBase's page and filter model",
            "Custom K2 wildcard nodes or arbitrary Blueprint request and response interceptors",
            "Multiple production transport backends before Unreal HTTP is proven on each supported target",
            "Schema-to-UStruct generation",
            "Offline dependency graphs, local ID mapping, automatic compaction, conflict merging, file staging, batch replay, offline reads, or a global record cache",
            "A broad per-endpoint capability taxonomy or portable connection-timeout promise"
          ),
          callout("note", "Deferred means no implied contract", "These ideas may be revisited after real shipped usage establishes a concrete need. Do not build game architecture around a promised arrival date."),
        ],
      },
      {
        id: "non-goals",
        title: "Global non-goals",
        blocks: [
          paragraph(
            "The runtime is a PocketBase Web API client, not an embedded PocketBase server. Realtime SSE is not a durable event log. The optional offline direction is a mutation outbox, not a local database. Privileged APIs are not for distributed game clients."
          ),
          bullets(
            "Response bytes, list traversal, memory files, retries, realtime queues, diagnostics, and progress rates stay bounded.",
            "Large values move instead of copy where the public Unreal API allows it.",
            "JSON is parsed once and retained.",
            "SDK locks are not held while broadcasting public delegates.",
            "Unknown response fields are retained instead of rejected."
          ),
        ],
      },
    ],
  },
  {
    slug: "reference/blueprint-nodes",
    title: "Blueprint node index",
    eyebrow: "Reference",
    description: "A category-by-category map of every public gameplay and trusted-tool Blueprint node in version 0.1.0.",
    readTime: "14 min",
    sections: [
      {
        id: "client",
        title: "Client and session nodes",
        blocks: [
          table(
            ["Node", "Kind", "Purpose"],
            [
              ["Initialize PocketBase", "Callable with success/failure paths", "Create the default Game Instance client from an origin"],
              ["Initialize PocketBase with Config", "Advanced callable", "Create the default client with full policy"],
              ["Get PocketBase Client", "Pure", "Retrieve the default client"],
              ["Create Named PocketBase Client", "Advanced callable", "Create a separate named client"],
              ["Get Named PocketBase Client", "Advanced pure", "Retrieve a named client"],
              ["Shutdown PocketBase", "Callable", "Remove and stop the default client"],
              ["Remove Named PocketBase Client", "Advanced callable", "Remove and stop one named client"],
              ["Is PocketBase Client Ready", "Pure", "Check whether a wrapper retains a live native client"],
              ["Get Base Url", "Pure", "Read the normalized configured origin"],
              ["Collection", "Pure", "Create a client plus collection value"],
              ["Get Capability / Get Capability Report", "Pure", "Read packaged feature support"],
              ["Is Authenticated", "Pure", "Check current auth presence"],
              ["Get Current Auth Record", "Callable with success/failure paths", "Read the current typed auth record"],
              ["Get Current Session", "Callable with success/failure paths", "Read the safe session snapshot"],
              ["Refresh Session", "Async", "Coordinate Auth Refresh"],
              ["Restore Session", "Async", "Load optional secure state and optionally verify it"],
              ["Logout", "Callable", "Immediately clear local auth state"],
              ["Shutdown", "Callable", "Stop one client wrapper and all retained work"]
            ]
          ),
          callout("note", "Two legacy nodes stay hidden for migration", "Get Current Auth Record (Legacy) and Get Current Session (Legacy) are deprecated. Replace them with the direct success and failure versions."),
        ],
      },
      {
        id: "records",
        title: "Record, body, and filter nodes",
        blocks: [
          table(
            ["Group", "Nodes"],
            [
              ["Requests", "Get Record; Get First Record; List Records; Get Full Record List; Create Record; Update Record; Delete Record"],
              ["Body builders", "New Record Body; With String Field; With Number Field; With Boolean Field; With Null Field; With String Array Field"],
              ["Field reads", "Has Field; Is Field Null; Try Get String, Integer, Number, Boolean, Date, String Array, and Object Field; Get String Field State; Get Integer Field State"],
              ["Projection", "Make Excerpt Field"],
              ["Dates", "Try Parse PocketBase Date; Format PocketBase Date"],
              ["Filters", "String Filter; Number Filter; Boolean Filter; Date Filter; Null Filter; And Filters; Or Filters; Raw Filter (Advanced)"],
              ["Batch builders", "New Batch; With Create; With Update; With Upsert; With Delete; Send Batch"]
            ]
          ),
          callout("note", "Builder nodes are pure", "With Field and With Operation nodes return a new value. They do not mutate an earlier wire in place."),
        ],
      },
      {
        id: "auth-files-realtime",
        title: "Authentication, files, realtime, and utilities",
        blocks: [
          table(
            ["Group", "Nodes"],
            [
              ["Login", "List Authentication Methods; Log In with Password; Request One-Time Password; Log In with One-Time Password"],
              ["OAuth2", "Begin Manual OAuth2; Complete Manual OAuth2; Log In with OAuth2"],
              ["Account", "Request Password Reset; Confirm Password Reset; Request Email Verification; Confirm Email Verification; Request Email Change; Confirm Email Change; List Linked External Auths; Unlink External Auth"],
              ["Uploads", "Create Record with Files; Update Record with Files"],
              ["Downloads", "Try Build File URL; Get Protected File Token; Download File"],
              ["Realtime", "Subscribe to Records; Subscribe to Record; Subscribe to Realtime Topic; Unsubscribe All Realtime; Subscription.Unsubscribe; Is Active; Get Connection State"],
              ["Routes", "Check Health; Send Custom Route"],
              ["Debug conversion", "Blueprint autocast To String nodes for public structs and enums, with credential-bearing values redacted"]
            ]
          ),
        ],
      },
      {
        id: "admin",
        title: "Development Only admin nodes",
        blocks: [
          paragraph("These nodes require OpenPocketBaseSDKAdmin and are excluded from Shipping by default."),
          table(
            ["Area", "Nodes"],
            [
              ["Client", "Initialize Privileged PocketBase; Is Ready; Is Authenticated; Logout; Shutdown"],
              ["Authentication", "Authenticate PocketBase Superuser"],
              ["Collections", "List, Get, Create, Update, Delete, and Import PocketBase Collections"],
              ["Settings", "Get PocketBase Settings; Update PocketBase Settings; Test PocketBase S3 Settings; Test PocketBase Email Settings"],
              ["Logs", "List PocketBase Logs; Get PocketBase Log"],
              ["Backups", "List, Create, Upload, Download, Restore, and Delete PocketBase Backup"],
              ["Crons", "List PocketBase Crons; Run PocketBase Cron"],
              ["Data", "Run PocketBase SQL; Impersonate PocketBase User"],
              ["Debug conversion", "Development Only To String autocasts for every admin result and policy type"]
            ]
          ),
          table(
            ["Exact Blueprint display name", "Area"],
            [
              ["List PocketBase Collections", "Collections"],
              ["Get PocketBase Collection", "Collections"],
              ["Create PocketBase Collection", "Collections"],
              ["Update PocketBase Collection", "Collections"],
              ["Delete PocketBase Collection", "Collections"],
              ["List PocketBase Backups", "Backups"],
              ["Create PocketBase Backup", "Backups"],
              ["Upload PocketBase Backup", "Backups"],
              ["Download PocketBase Backup", "Backups"],
              ["Restore PocketBase Backup", "Backups"]
            ]
          ),
        ],
      },
    ],
  },
  {
    slug: "reference/data-types",
    title: "Data type reference",
    eyebrow: "Reference",
    description: "Find the main structs and enums by workflow, including the fields that control bounds, lifecycle, and response meaning.",
    readTime: "16 min",
    sections: [
      {
        id: "core-types",
        title: "Core and request types",
        blocks: [
          table(
            ["Type", "Key fields or values"],
            [
              ["FOpenPocketBaseClientConfig", "BaseUrl, ProfileName, AcceptLanguage, DefaultHeaders, SessionPersistence, proactive refresh policy, assisted OAuth gate"],
              ["FOpenPocketBaseRequestOptions", "Total and activity timeout, request key replacement, headers, traceparent, retry bounds, MaxResponseBytes"],
              ["FOpenPocketBaseError", "Kind, HttpStatus, ServerCode, ServerMessage, FieldErrors, bMayRetry, RequestId"],
              ["EOpenPocketBaseErrorKind", "None, Cancelled, InvalidArgument, Transport, Timeout, Http, PocketBase, Serialization, Authentication, SecureStorage, OfflineQueue, Unsupported, Internal"],
              ["FOpenPocketBaseCapabilityInfo", "Capability, Status, Platform, BuildConfiguration, Reason"],
              ["FOpenPocketBaseCapabilityReport", "Entries"],
              ["FOpenPocketBaseRequestHandle", "Cancel and validity for one native request"]
            ]
          ),
        ],
      },
      {
        id: "record-types",
        title: "Record and query types",
        blocks: [
          table(
            ["Type", "Key fields or values"],
            [
              ["FOpenPocketBaseRecordBody", "Dynamic Data sent in mutations"],
              ["FOpenPocketBaseRecord", "Id, CollectionId, CollectionName, Created, Updated, Data, Expanded"],
              ["EOpenPocketBaseFieldModifier", "Replace, Append, Prepend, Remove"],
              ["EOpenPocketBaseFieldState", "Found, Missing, Null, WrongType"],
              ["FOpenPocketBaseRecordOptions", "Expand, Fields, RequestOptions"],
              ["FOpenPocketBaseListOptions", "Page, PerPage, Filter, Sort, Expand, Fields, bSkipTotal, RequestOptions"],
              ["FOpenPocketBaseFullListOptions", "ListOptions, MaxItems, MaxPages"],
              ["FOpenPocketBaseRecordPage", "Page, PerPage, Items, optional total flags and values"],
              ["FOpenPocketBaseFullListResult", "Items, PagesFetched, bReachedEnd, bReachedItemLimit, bReachedPageLimit"],
              ["FOpenPocketBaseFilter", "Expression, bValid, ErrorMessage"],
              ["Filter comparison enums", "Typed scalar and Any comparisons for string, number, boolean, date, and null"]
            ]
          ),
        ],
      },
      {
        id: "auth-types",
        title: "Authentication and session types",
        blocks: [
          table(
            ["Type", "Key fields or values"],
            [
              ["FOpenPocketBaseAuthMethods", "Mfa, Otp, Password, OAuth2 capability details"],
              ["FOpenPocketBaseAuthAttempt", "Status, Authentication, Mfa"],
              ["EOpenPocketBaseAuthAttemptStatus", "Authenticated, MfaRequired"],
              ["FOpenPocketBaseOAuth2StartOptions", "Provider, RedirectUrl, Scopes, RequestOptions"],
              ["FOpenPocketBaseOAuth2Authorization", "TransactionId, AuthorizationUrl, state and PKCE metadata, ExpiresAtUtc"],
              ["FOpenPocketBaseOAuth2Callback", "TransactionId, CallbackUrl, CreateData, Mfa, RequestOptions"],
              ["FOpenPocketBaseAssistedOAuth2Options", "Provider, Scopes, CreateData, Mfa, RequestOptions"],
              ["FOpenPocketBaseSessionSnapshot", "bAuthenticated, AuthCollection, AuthGeneration, PersistenceState, Reason, AuthRecord"],
              ["EOpenPocketBaseSessionChangeReason", "LoggedIn, LoggedOut, Refreshed, Restored, UserSwitched, RecordUpdated"],
              ["FOpenPocketBaseSessionRestoreResult", "Status, Session"],
              ["EOpenPocketBaseSessionRestoreStatus", "Restored, Verified, NotFound, Expired, Corrupt, Unavailable, PolicyRejected"]
            ]
          ),
        ],
      },
      {
        id: "file-live-types",
        title: "File, batch, route, and realtime types",
        blocks: [
          table(
            ["Type", "Key fields or values"],
            [
              ["FOpenPocketBaseFileInput", "FieldName, FileName, ContentType, Modifier, source selection, FilePath, Bytes"],
              ["FOpenPocketBaseUploadLimits", "MaxFiles, MaxInlineFileBytes, MaxSourceFileBytes, MaxTotalBodyBytes"],
              ["FOpenPocketBaseTransferProgress", "TransferredBytes, optional total, Attempt, Phase"],
              ["FOpenPocketBaseFileDownloadOptions", "Target, DestinationPath, replace policy, MaxBytes, UrlOptions, RequestOptions"],
              ["FOpenPocketBaseFileDownloadResult", "Bytes or destination plus sanitized HTTP metadata"],
              ["FOpenPocketBaseBatchRequest", "Ordered Entries"],
              ["FOpenPocketBaseBatchOptions", "MaxOperations, MaxBodyBytes, RequestOptions"],
              ["FOpenPocketBaseCustomRouteRequest", "Method, Path, Query, auth, one body format, bounds, Options"],
              ["FOpenPocketBaseCustomRouteResponse", "Status, content type, request ID, bytes, parsed JSON shape, duration"],
              ["FOpenPocketBaseRealtimeOptions", "Filter, Expand, Fields, bounded query parameters and headers"],
              ["FOpenPocketBaseRealtimeEvent", "Topic, typed and original action, optional Record, additional Data"],
              ["EOpenPocketBaseRealtimeConnectionState", "Created, Subscribing, Active, Reconnecting, Stopped"]
            ]
          ),
        ],
      },
      {
        id: "admin-types",
        title: "Admin types",
        blocks: [
          table(
            ["Type", "Key fields"],
            [
              ["FOpenPocketBaseAdminPolicy", "All privileged opt-ins plus page, request, response, backup, and SQL row bounds"],
              ["FOpenPocketBaseAdminListOptions", "Page, PerPage, raw trusted Filter, Sort, Fields, RequestOptions"],
              ["FOpenPocketBaseAdminDocument", "Version-dependent JSON Data"],
              ["FOpenPocketBaseAdminPage", "Page metadata and JSON Items"],
              ["FOpenPocketBaseAdminBackup", "Key, Size, Modified"],
              ["FOpenPocketBaseAdminBackupDownload", "Bytes, ContentType"],
              ["FOpenPocketBaseAdminSqlResult", "Execution time, affected rows, row count, column metadata, JSON Data"],
              ["FOpenPocketBaseAdminIdentity", "Authenticated superuser Id and Email"]
            ]
          ),
        ],
      },
    ],
  },
  {
    slug: "reference/api-index",
    title: "C++ API index",
    eyebrow: "Reference",
    description: "The public native entry points, grouped by service, with the header and result shape to reach for.",
    readTime: "12 min",
    sections: [
      {
        id: "client-api",
        title: "FOpenPocketBaseClient",
        blocks: [
          code("cpp", `#include "OpenPocketBaseClient.h"`),
          table(
            ["Member", "Returns", "Purpose"],
            [
              ["Create", "FOpenPocketBaseClientResult", "Validate config and dependencies, then create a client"],
              ["Collection", "FOpenPocketBaseCollectionService", "Create a lightweight service for one collection"],
              ["Files", "FOpenPocketBaseFileService", "Create the file URL, token, and download service"],
              ["Health", "FOpenPocketBaseRequestHandle", "Call /api/health"],
              ["SendCustomRoute", "FOpenPocketBaseRequestHandle", "Call one validated project route"],
              ["GetBaseUrl", "FString", "Read normalized origin"],
              ["GetCapability / GetCapabilityReport", "Capability value", "Read packaged support"],
              ["IsAuthenticated", "bool", "Check current auth presence"],
              ["GetCurrentAuthRecord / GetCurrentSession", "bool", "Copy a safe current snapshot"],
              ["OnSessionChanged", "delegate reference", "Observe ordered session changes"],
              ["RefreshAuth / RestoreSession", "request handle", "Coordinate session refresh or restore"],
              ["Logout", "void", "Clear current auth locally"],
              ["SendBatch", "request handle", "Send an explicit transaction"],
              ["Subscribe", "subscription result", "Subscribe to an advanced realtime topic"],
              ["UnsubscribeTopic / UnsubscribeAllRealtime", "void", "Stop realtime topics"],
              ["IsShutdown / Shutdown", "bool / void", "Inspect or stop the client lifecycle"]
            ]
          ),
        ],
      },
      {
        id: "collection-api",
        title: "FOpenPocketBaseCollectionService",
        blocks: [
          table(
            ["Area", "Members"],
            [
              ["Read", "GetOne, GetList, GetFullList, GetFirstListItem"],
              ["Write", "Create, CreateWithFiles, Update, UpdateWithFiles, Delete"],
              ["Auth discovery", "ListAuthMethods, RequestOtp"],
              ["Login", "AuthWithPassword, AuthWithOtp, BeginOAuth2, CompleteOAuth2, AuthWithOAuth2"],
              ["Account", "RequestPasswordReset, ConfirmPasswordReset, RequestVerification, ConfirmVerification, RequestEmailChange, ConfirmEmailChange"],
              ["External auth", "ListExternalAuths, UnlinkExternalAuth"],
              ["Realtime", "SubscribeToRecords, SubscribeToRecord"],
              ["State", "IsValid"]
            ]
          ),
          paragraph("Every network member returns FOpenPocketBaseRequestHandle. Every callback receives TOpenPocketBaseResult<T> for its typed response."),
        ],
      },
      {
        id: "file-api",
        title: "FOpenPocketBaseFileService",
        blocks: [
          table(
            ["Member", "Result"],
            [
              ["BuildUrl", "FOpenPocketBaseFileUrlResult with a public URL or validation error"],
              ["GetToken", "Request callback containing opaque FOpenPocketBaseFileToken"],
              ["Download", "Request callback containing FOpenPocketBaseFileDownloadResult plus optional progress"]
            ]
          ),
          paragraph("CreateWithFiles and UpdateWithFiles remain collection members because uploads are record mutations."),
        ],
      },
      {
        id: "libraries",
        title: "Blueprint-facing native libraries",
        blocks: [
          table(
            ["Header", "Class", "Responsibility"],
            [
              ["OpenPocketBaseClientLibrary.h", "UOpenPocketBaseClientLibrary", "Default and named Game Instance clients"],
              ["OpenPocketBaseRecordLibrary.h", "UOpenPocketBaseRecordLibrary", "Body builders, field access, excerpts, date conversion"],
              ["OpenPocketBaseFilterLibrary.h", "UOpenPocketBaseFilterLibrary", "Typed filters and composition"],
              ["OpenPocketBaseFileLibrary.h", "UOpenPocketBaseFileLibrary", "Validated public file URL"],
              ["OpenPocketBaseBatchLibrary.h", "UOpenPocketBaseBatchLibrary", "Pure batch builders"],
              ["OpenPocketBaseRealtimeLibrary.h", "UOpenPocketBaseRealtimeLibrary", "Blueprint subscription creation and stop-all"],
              ["OpenPocketBaseStringLibrary.h", "UOpenPocketBaseStringLibrary", "Sanitized Blueprint debug autocasts"],
              ["AsyncActions/OpenPocketBaseRecordAsyncActions.h", "UOpenPocketBase*AsyncAction", "Health, routes, records, auth, account, restore, and uploads"],
              ["AsyncActions/OpenPocketBaseFileAsyncActions.h", "UOpenPocketBase*File*AsyncAction", "Protected token and downloads"],
              ["AsyncActions/OpenPocketBaseBatchAsyncAction.h", "UOpenPocketBaseSendBatchAsyncAction", "Transactions"]
            ]
          ),
        ],
      },
      {
        id: "admin-api",
        title: "FOpenPocketBaseAdminClient",
        blocks: [
          code("cpp", `#include "OpenPocketBaseAdminClient.h"`),
          table(
            ["Area", "Members"],
            [
              ["Lifecycle", "Create, IsAuthenticated, Logout, Shutdown"],
              ["Authentication", "AuthenticateSuperuser"],
              ["Collections", "ListCollections, GetCollection, CreateCollection, UpdateCollection, DeleteCollection, ImportCollections"],
              ["Settings", "GetSettings, UpdateSettings, TestS3, TestEmail"],
              ["Logs", "ListLogs, GetLog"],
              ["Backups", "ListBackups, CreateBackup, UploadBackup, DownloadBackup, RestoreBackup, DeleteBackup"],
              ["Crons", "ListCrons, RunCron"],
              ["SQL", "RunSql"],
              ["Impersonation", "Impersonate"]
            ]
          ),
          callout("warning", "Development Only by default", "OpenPocketBaseSDKAdmin is a separate module with a Shipping deny entry. Its runtime policy is an additional gate, not a substitute for build isolation."),
        ],
      },
    ],
  },
];
