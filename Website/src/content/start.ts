import { bullets, callout, cards, code, lead, paragraph, screenshot, steps, table, type DocPage } from "./types";

export const startPages: DocPage[] = [
  {
    slug: "overview",
    title: "PocketBase for Unreal, without the glue code",
    eyebrow: "OpenMobile / OpenPocketBase",
    description:
      "A practical C++ and Blueprint client for records, authentication, files, realtime updates, custom routes, and trusted PocketBase tooling.",
    readTime: "6 min",
    sections: [
      {
        id: "what-you-get",
        title: "One client, the full game-facing workflow",
        blocks: [
          lead(
            "OpenPocketBase SDK turns PocketBase's Web API into Unreal-native types, cancellable requests, and Blueprint nodes. You keep the backend small and move through the same workflow in C++ or Blueprint without rebuilding HTTP, JSON, auth state, file streaming, and reconnect logic yourself."
          ),
          cards(
            { title: "Records", text: "Create, read, update, delete, filter, expand, project, paginate, and transact.", link: "records/crud", label: "DATA" },
            { title: "Authentication", text: "Password, OTP, MFA continuation, OAuth2, account actions, refresh, and logout.", link: "authentication/password-otp", label: "IDENTITY" },
            { title: "Files", text: "Multipart record mutations, public URLs, protected tokens, and bounded streaming downloads.", link: "files/uploads", label: "TRANSFER" },
            { title: "Realtime", text: "Owned subscriptions with state, errors, reconnection, and gap signaling.", link: "realtime/subscriptions", label: "LIVE" },
            { title: "Project routes", text: "Health checks and typed custom HTTP routes that keep the SDK's safety rules.", link: "tools/custom-routes", label: "EXTEND" },
            { title: "Admin tools", text: "A separate non-Shipping module for collections, logs, backups, crons, SQL, and impersonation.", link: "tools/admin-api", label: "TRUSTED" }
          ),
        ],
      },
      {
        id: "compatibility",
        title: "Know the contract before you ship",
        blocks: [
          paragraph(
            "Version 0.1.0 targets PocketBase v0.39.11 exactly. PocketBase is still pre-1.0, so a newer server is not treated as compatible until its routes and behavior have been checked against the repository's pinned integration suite. Unknown response fields are retained, but that does not replace a compatibility pass."
          ),
          table(
            ["Area", "Status", "What that means"],
            [
              ["HTTP records and auth", "Implemented", "Available through the core runtime module."],
              ["Secure persistence", "Platform-gated", "Apple Keychain is implemented on Mac and iOS. Packaged proof currently covers Mac ARM64."],
              ["Streaming and realtime", "Platform-gated", "Packaged proof currently covers Mac ARM64."],
              ["Privileged API", "Development only", "The complete admin module is excluded from Shipping by default."],
              ["Offline outbox", "Optional module", "Never enabled by ordinary record calls and never changes their semantics."]
            ]
          ),
          callout(
            "note",
            "Independent project",
            "OpenPocketBase SDK is community-built. It is not affiliated with, endorsed by, or maintained by the PocketBase project."
          ),
        ],
      },
      {
        id: "choose-a-path",
        title: "Choose your path",
        blocks: [
          steps(
            { title: "Blueprint project", text: "Install the plugin, initialize the default client, create a collection value, then use focused async actions." },
            { title: "C++ project", text: "Add the runtime module dependency, create a validated client config, and retain request handles only when cancellation is needed." },
            { title: "Already integrated", text: "Jump to records, authentication, files, or realtime and use the Blueprint and C++ tabs side by side." }
          ),
          screenshot("[put a screenshot of the finished OpenMobile PocketBase overview page here]"),
        ],
      },
    ],
  },
  {
    slug: "start/installation",
    title: "Install the plugin",
    eyebrow: "Start here",
    description: "Place the plugin in your Unreal project, enable it, and add the right modules without pulling trusted tooling into a game build.",
    readTime: "5 min",
    sections: [
      {
        id: "project-install",
        title: "Project installation",
        blocks: [
          steps(
            { title: "Create the Plugins folder", text: "If the project does not already have one, create YourProject/Plugins beside the .uproject file." },
            { title: "Copy the repository", text: "The final path should be YourProject/Plugins/OpenPocketBaseSDK/OpenPocketBaseSDK.uplugin." },
            { title: "Regenerate and reopen", text: "Regenerate C++ project files when applicable, then reopen Unreal Editor." },
            { title: "Enable the plugin", text: "Open Edit > Plugins, find Open PocketBase SDK in Online, enable it, and restart the editor if asked." }
          ),
          screenshot("[put a screenshot of Open PocketBase SDK enabled in Unreal's Plugins window here]"),
          callout("warning", "Use an HTTPS origin outside local development", "HTTP is useful for a local PocketBase process. A shipped build should talk to a trusted HTTPS origin."),
        ],
      },
      {
        id: "module-dependencies",
        title: "Add the runtime module",
        blocks: [
          paragraph("Blueprint-only projects can stop after enabling the plugin. A C++ module that includes SDK headers must declare the core module in its Build.cs file."),
          code(
            "csharp",
            `PublicDependencyModuleNames.AddRange(new[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "OpenPocketBaseSDK"
});`,
            "YourGame.Build.cs"
          ),
          table(
            ["Module", "Add it when", "Shipping"],
            [
              ["OpenPocketBaseSDK", "The game uses records, auth, files, realtime, or custom routes.", "Included"],
              ["OpenPocketBaseSDKOffline", "You explicitly adopt the optional offline outbox.", "Optional"],
              ["OpenPocketBaseSDKAdmin", "A trusted editor, operator, or server target uses privileged APIs.", "Denied by default"],
              ["OpenPocketBaseSDKEditor", "Loaded by the plugin for editor validation.", "Editor only"]
            ]
          ),
          callout(
            "danger",
            "Do not add the admin module to an ordinary client",
            "It contains privileged routes and is excluded from Shipping by the plugin descriptor. Keep it in trusted tools or server-side targets."
          ),
        ],
      },
      {
        id: "verify",
        title: "Verify the install",
        blocks: [
          bullets(
            "The Open PocketBase SDK section appears under Project Settings > Plugins.",
            "Blueprint search finds Initialize PocketBase with Context Sensitive enabled.",
            "A C++ file can include OpenPocketBaseClient.h after the Build.cs change.",
            "The Output Log does not report a missing runtime module during editor startup."
          ),
          screenshot("[put a screenshot of Initialize PocketBase in the Blueprint action menu here]"),
        ],
      },
    ],
  },
  {
    slug: "start/quickstart",
    title: "Your first PocketBase request",
    eyebrow: "Start here / Blueprint",
    description: "Import your PocketBase schema, initialize the client, check server health, and prove the failure path without guessing any pins.",
    readTime: "18 min",
    sections: [
      {
        id: "what-we-are-checking",
        title: "What we're checking",
        blocks: [
          lead(
            "We're gonna start with the boring foundation because every later graph depends on it. You'll import a real schema, connect one client, pick a collection without typing its name, run Check Health, print the SDK values directly, then stop PocketBase and make sure the same graph reports a useful error. That itself is plenty for the first pass."
          ),
          bullets(
            "The schema asset remembers stable PocketBase collection and field IDs.",
            "The project profile tells editor pickers which schema to show when a graph has no collection context yet.",
            "Initialize PocketBase creates the default Game Instance client.",
            "Check Health proves the configured origin can answer a real request.",
            "The failed run proves transport errors reach the Failed exec pin as an Open Pocket Base Error."
          ),
          callout(
            "note",
            "Use one local origin for the whole page",
            "This walkthrough uses http://127.0.0.1:18091 because that's the repository fixture server. If your project uses another port, replace it everywhere on this page. Don't initialize one port and inspect another PocketBase process, that looks harmless only and wastes a lot of time."
          ),
        ],
      },
      {
        id: "server-first",
        title: "Start the fixture server first",
        blocks: [
          paragraph(
            "Open PowerShell in your Unreal project folder and start the fixture. The script keeps its PID, log files, data folder, and migration folder together, so you don't have to keep a random terminal window around."
          ),
          code(
            "powershell",
            `& ".\\PocketBase\\Start-PocketBase.ps1"`,
            "Start the local fixture"
          ),
          paragraph(
            "A successful start prints the origin plus the seeded player email and password. Open http://127.0.0.1:18091/api/health in a browser if you want one check outside Unreal also. You should get a healthy response from PocketBase v0.39.11."
          ),
          callout(
            "warning",
            "Don't copy fixture credentials into assets",
            "The seeded login belongs to local testing. Later auth pages pass passwords from live input. A Blueprint default gets serialized into the asset, so we're not gonna put a password there and pretend it's fine."
          ),
        ],
      },
      {
        id: "import-schema",
        title: "Import the PocketBase schema",
        blocks: [
          paragraph(
            "Get the collection response from a trusted PocketBase admin flow and save it as JSON. The importer accepts a root collection array, an object with an items array, or an object with a collections array. For the repository fixture, keep the file at YourProject/PocketBase/openpocketbase_schema.json so Refresh Schema can find the same source later."
          ),
          steps(
            { title: "Make a Content folder", text: "Create a PocketBase folder in the Content Browser. Keeping the schema easy to find helps once your project has more than one profile." },
            { title: "Drag in the JSON file", text: "Drop openpocketbase_schema.json into that folder. A valid PocketBase response imports directly as a PocketBase Schema asset. The DataTable Options window shouldn't appear." },
            { title: "Rename the asset", text: "Rename the imported asset to PB_FixtureSchema for this walkthrough. The name is local to Unreal, PocketBase IDs stay inside the asset." },
            { title: "Keep the source file", text: "Don't delete or move the JSON after import. Preview Changes and Refresh Schema read that path again." }
          ),
          screenshot("[put a screenshot of the imported PB_FixtureSchema asset in the Content Browser here]"),
          callout(
            "note",
            "What the fixture import should show",
            "The repository fixture has two collections, sdk_tasks is base and sdk_users is auth. Its current asset summary shows 17 fields, 9 writable fields, and 6 required fields. Your own schema will have different counts, no problem."
          ),
        ],
      },
      {
        id: "inspect-schema",
        title: "Check the imported asset before touching Blueprint",
        blocks: [
          paragraph(
            "Double-click PB_FixtureSchema. Read the summary and validation text at the top of its Details panel before you trust any dropdown. For the fixture, you should see the values below."
          ),
          table(
            ["Check", "Expected fixture value"],
            [
              ["Collections", "2 total, 1 base, 1 auth, 0 view"],
              ["Fields", "17 total, 9 writable, 6 required"],
              ["Source", "Ready"],
              ["Fingerprint", "Ready"],
              ["Validation", "Schema validation passed."]
            ]
          ),
          steps(
            { title: "Click Preview Changes", text: "The source hasn't changed yet, so the dialog should say No schema changes. The asset stays untouched." },
            { title: "Click Refresh Schema", text: "The same source is already imported, so Unreal should report that the PocketBase schema is already current." },
            { title: "Save the asset", text: "Save it now. An unsaved schema works in the current editor session, then disappears on restart, which isn't the test we're trying to run." }
          ),
          screenshot("[put a screenshot of the schema summary validation and refresh controls here]"),
        ],
      },
      {
        id: "profile",
        title: "Assign the schema to a project profile",
        blocks: [
          paragraph(
            "Open Edit > Project Settings > Plugins > Open PocketBase SDK. Add one Profiles entry and fill it in exactly as shown below. Set Default Profile after the entry exists, otherwise Unreal can leave you with a name that doesn't resolve to anything."
          ),
          table(
            ["Setting", "Fixture value"],
            [
              ["Default Profile", "Local"],
              ["Profiles entry Name", "Local"],
              ["Base URL", "http://127.0.0.1:18091"],
              ["Schema", "PB_FixtureSchema"],
              ["Session Persistence", "Memory Only"]
            ]
          ),
          paragraph(
            "The profile scopes editor pickers when the graph doesn't already carry a collection. Initialize PocketBase still takes its Base URL input on this page. Yes, those are separate jobs, and I'm saying it directly because hiding that detail would make the first graph confusing."
          ),
          screenshot("[put a screenshot of the Local Open PocketBase project profile with its schema assigned here]"),
        ],
      },
      {
        id: "make-blueprint",
        title: "Create a clean Blueprint for the check",
        blocks: [
          steps(
            { title: "Create an Actor Blueprint", text: "Name it BP_PB_Chunk01. An Actor gives this page a simple BeginPlay owner without changing your Game Instance yet." },
            { title: "Place one instance", text: "Drag BP_PB_Chunk01 into the level you're gonna Play. Creating the asset itself won't run BeginPlay." },
            { title: "Open Event Graph", text: "Delete any nodes you don't need, then keep Event BeginPlay." },
            { title: "Add Initialize PocketBase", text: "Set Base URL to http://127.0.0.1:18091. Its World Context is resolved automatically and that pin stays hidden." },
            { title: "Add Check Health", text: "This async node has Pocket Base Client and Options inputs. Options stays visible, leave its default value for now." },
            { title: "Add Use Collection", text: "Drag from Initialize PocketBase's blue Client output and search for Use Collection. It's a pure node, so it won't have exec pins." },
            { title: "Add five Print String nodes", text: "Two belong to the successful health path. The other three show health failure, cancellation, and initialization failure." }
          ),
          callout(
            "warning",
            "Don't add a subsystem node here",
            "This flow doesn't use Get Game Instance Subsystem. Initialize PocketBase returns the client directly, and Get PocketBase Client is available in other Blueprints after initialization."
          ),
          screenshot("[put a screenshot of the seven Chunk 1 Blueprint nodes before wiring here]"),
        ],
      },
      {
        id: "collection-picker",
        title: "Test the collection picker before wiring requests",
        blocks: [
          paragraph(
            "Click the Reference input on Use Collection. The picker should show sdk_tasks and sdk_users from PB_FixtureSchema. We haven't connected an operation yet, so both readable collection types can appear."
          ),
          steps(
            { title: "Search for tasks", text: "Type tasks in the picker search. Only sdk_tasks should remain." },
            { title: "Select sdk_tasks", text: "The pin label should change from Choose collection to sdk_tasks." },
            { title: "Clear it once", text: "Open the picker again and click Clear selection. The pin should return to Choose collection without editing any struct text." },
            { title: "Select sdk_tasks again", text: "We need the collection set for the Print String check below." }
          ),
          paragraph(
            "If the picker is empty, stop there. Check that the schema asset was saved and assigned to Local. Typing sdk_tasks into a plain string pin would avoid the feature we're trying to verify, so we're not gonna do that."
          ),
          screenshot("[put a screenshot of the searchable collection picker showing sdk_tasks and sdk_users here]"),
        ],
      },
      {
        id: "wire-exec",
        title: "Wire the execution flow in this order",
        blocks: [
          paragraph(
            "Start with the white exec wires only. Data pins come immediately after, once the request order is visible."
          ),
          code(
            "text",
            `Event BeginPlay
    -> Initialize PocketBase

Initialize PocketBase True
    -> Check Health

Check Health Success
    -> Print String 1
    -> Print String 2

Check Health Failed
    -> Print String 3

Check Health Cancelled
    -> Print String 4

Initialize PocketBase False
    -> Print String 5`,
            "Execution wires"
          ),
          paragraph(
            "Initialize PocketBase uses True and False exec outputs because its Boolean result expands into execution pins. Check Health uses Success, Failed, and Cancelled because it's an async request. Don't add a Branch between either of them."
          ),
        ],
      },
      {
        id: "wire-data",
        title: "Connect the data pins after the exec flow",
        blocks: [
          code(
            "text",
            `Initialize PocketBase Client
    -> Check Health Pocket Base Client

Check Health Health Result
    -> Print String 1 In String

Use Collection Return Value
    -> Print String 2 In String

Check Health Error
    -> Print String 3 In String

Check Health Error
    -> Print String 4 In String

Initialize PocketBase Error
    -> Print String 5 In String`,
            "Data wires"
          ),
          paragraph(
            "Connect the SDK structs straight into Print String. Unreal inserts the OpenPocketBase conversion node automatically and prints readable JSON. Don't split Health Result into five fields and don't rebuild it with Format Text, we've already got a formatter for that stuff."
          ),
          callout(
            "note",
            "One Error pin can feed both terminal prints",
            "Failed and Cancelled share the Check Health Error output. Each exec path reads the value produced for that terminal result. The cancellation wire is present now, but we'll trigger cancellation in its own page later."
          ),
          screenshot("[put a screenshot of the complete Chunk 1 Blueprint graph with exec and data wires here]"),
        ],
      },
      {
        id: "successful-run",
        title: "Run the healthy server case",
        blocks: [
          steps(
            { title: "Compile and save", text: "Fix any red Blueprint pin before Play. A successful compile also proves the imported references are current." },
            { title: "Press Play", text: "The fixture is already running, so Check Health should finish on Success." },
            { title: "Read Output Log", text: "Print String 1 shows Open Pocket Base Health Result. Print String 2 shows the selected sdk_tasks collection value." }
          ),
          table(
            ["Health field", "Expected value"],
            [
              ["healthy", "true"],
              ["httpStatus", "200"],
              ["code", "200"],
              ["message", "API is healthy."],
              ["durationSeconds", "A small non-negative value"]
            ]
          ),
          paragraph(
            "The collection print should include sdk_tasks plus pbc_4257125328 when you're using the repository fixture. Seeing the collection after the health print proves the pure Use Collection value can flow into ordinary Blueprint debugging also."
          ),
        ],
      },
      {
        id: "failed-run",
        title: "Run the server-down case",
        blocks: [
          paragraph(
            "What happens when PocketBase is down? Initialize PocketBase can still take its True path because it validates and creates the client locally. Check Health performs the network request, waits through its configured retry policy, then fires Failed."
          ),
          code(
            "powershell",
            `& ".\\PocketBase\\Stop-PocketBase.ps1"`,
            "Stop the fixture"
          ),
          steps(
            { title: "Play again", text: "Wait a few seconds if the default retry policy is active. Print String 3 should fire when the request finishes failing." },
            { title: "Read the error", text: "The result should be an Open Pocket Base Error with kind Transport, HTTP status 0, a request ID, and mayRetry set to true." },
            { title: "Check the graph stayed alive", text: "There should be no Accessed None warning, assertion, or editor crash. A dead local server is an ordinary request failure." }
          ),
          code(
            "powershell",
            `& ".\\PocketBase\\Start-PocketBase.ps1"`,
            "Start the fixture again"
          ),
          paragraph(
            "Start the fixture again before moving on. Later record and auth pages assume 18091 is answering, and leaving it stopped creates unrelated failures all over the place."
          ),
        ],
      },
      {
        id: "troubleshooting",
        title: "If your result doesn't match",
        blocks: [
          table(
            ["What you see", "What to check"],
            [
              ["DataTable Options opens", "The generic JSON importer won. Confirm you're using the current plugin build, then check that the JSON contains a collection array with id, name, type, and fields."],
              ["Schema Source says Missing", "Put the JSON back at the source path or reimport from its new path."],
              ["Collection picker is empty", "Save the schema asset, assign it to the Local profile, and reopen the Blueprint pin menu."],
              ["Use Collection has a text Name pin", "You've placed an old or dynamic node. Remove it and add Use Collection from the Client output."],
              ["Check Health asks for World Context Object", "The copied plugin is stale. The current node takes Pocket Base Client and Options only."],
              ["Accessed None appears", "Use the Client output from the same Initialize PocketBase node. Don't fetch a named client that was never created."],
              ["Health reaches port 8090", "Replace the Base URL with http://127.0.0.1:18091 for the repository fixture."],
              ["Print String won't accept the struct", "Remove any manually placed conversion, drag the SDK struct pin directly into In String, and let Blueprint add the autocast."]
            ]
          ),
        ],
      },
      {
        id: "pass-checklist",
        title: "Call this page passed only after all of these work",
        blocks: [
          bullets(
            "The JSON imports through PocketBase Schema and the asset validation passes.",
            "Preview Changes reports no changes and Refresh Schema reports that the asset is current.",
            "The Local profile retains PB_FixtureSchema after closing Project Settings.",
            "The collection picker searches, clears, and selects sdk_tasks without typed collection names.",
            "Initialize PocketBase reaches True and Check Health reaches Success while the fixture is running.",
            "Health Result and Use Collection connect directly to Print String.",
            "Check Health reaches Failed with a Transport error while the fixture is stopped.",
            "Neither run produces an Accessed None warning, assertion, or editor crash."
          ),
          paragraph(
            "If one item fails, send the Output Log plus a screenshot of that node. Don't rebuild the whole graph yet. We'll fix the actual failing part, copy the plugin again after Unreal closes, then repeat only this page."
          ),
        ],
      },
      {
        id: "next",
        title: "Continue after this page passes",
        blocks: [
          cards(
            { title: "Authentication", text: "Use sdk_users, log in, inspect the auth result, read session state, then log out cleanly.", link: "authentication/password-otp" },
            { title: "Record operations", text: "Use sdk_tasks for typed create, read, update, delete, filters, sorting, and projections.", link: "records/crud" },
            { title: "Request behavior", text: "Trigger cancellation, timeouts, retry decisions, and one-terminal-result handling.", link: "core/requests-errors" }
          ),
        ],
      },
    ],
  },
  {
    slug: "start/blueprint-basics",
    title: "Blueprint mental model",
    eyebrow: "Start here / Blueprint",
    description: "Understand clients, collection values, async actions, value builders, advanced pins, and cancellation before wiring a larger graph.",
    readTime: "7 min",
    sections: [
      {
        id: "three-values",
        title: "The three values you pass around",
        blocks: [
          table(
            ["Value", "Owns", "Use it for"],
            [
              ["Open Pocket Base Client", "Connection policy, auth state, active requests, realtime manager", "Health, session, files, custom routes, and creating collection values"],
              ["Open Pocket Base Collection", "A retained client plus one schema collection reference", "Record and auth actions"],
              ["Open Pocket Base Subscription", "One realtime registration and its delegates", "Listening, observing state, and unsubscribing"]
            ]
          ),
          paragraph(
            "Use Collection is a pure node. Pick the collection from your imported schema and pass its output directly into record and auth actions. It doesn't send a request or copy the client. The stored collection ID also survives a rename, so you don't have to chase typed names through every Blueprint later."
          ),
          screenshot("[put a screenshot of Client, Collection, and Subscription Blueprint values compared side by side here]"),
        ],
      },
      {
        id: "async-actions",
        title: "Async actions have one terminal result",
        blocks: [
          paragraph(
            "Network nodes expose Success, Failed, and Cancelled. Exactly one fires. Failed carries FOpenPocketBaseError, while Cancelled represents an explicit Cancel call, client shutdown, or another request replacing the same request key."
          ),
          bullets(
            "Promote Async Action to a variable only when the caller may cancel it.",
            "Destroying or dropping the proxy does not define cancellation. Call Cancel explicitly.",
            "Progress may fire many times, but never after a terminal output.",
            "The collection value retains its client, so async nodes do not need a World Context Object pin."
          ),
          screenshot("[put a screenshot of an async action proxy promoted to a variable and its Cancel call here]"),
        ],
      },
      {
        id: "value-builders",
        title: "Pure builders return new values",
        blocks: [
          paragraph(
            "New Record Body starts from the selected writable collection. With String Field and the other typed field nodes then show fields from that same schema context. Filter nodes, New Batch, and With Create follow the same value-builder flow. Each step returns a new struct, so the graph doesn't depend on some hidden mutation order."
          ),
          screenshot("[put a screenshot of New Record Body chained through three With Field nodes into Create Record here]"),
          callout("note", "Advanced pins stay out of the way", "Expand advanced pins when you need projections, request policy, bounds, upload limits, or retries. Defaults are designed for the ordinary path."),
        ],
      },
      {
        id: "events-and-lifetime",
        title: "Choose an owner with a clear lifetime",
        blocks: [
          paragraph(
            "The default client belongs to the Game Instance subsystem. Initialize it once, retrieve it elsewhere with Get PocketBase Client, and call Shutdown PocketBase during deterministic teardown if you need an earlier stop. Game Instance teardown shuts down every remaining default and named client."
          ),
          bullets(
            "Bind Session Changed where UI or gameplay must react to login, refresh, restore, record synchronization, or logout.",
            "Keep realtime subscription objects in the system that wants the updates and unsubscribe when that system stops.",
            "Use named clients only for genuinely separate PocketBase origins or separate auth stores."
          ),
        ],
      },
    ],
  },
  {
    slug: "start/cpp-basics",
    title: "C++ mental model",
    eyebrow: "Start here / C++",
    description: "Use result values, shared client references, lightweight services, request handles, and game-thread callbacks correctly.",
    readTime: "7 min",
    sections: [
      {
        id: "create",
        title: "Create once and keep the shared reference",
        blocks: [
          code(
            "cpp",
            `#include "OpenPocketBaseClient.h"

FOpenPocketBaseClientConfig Config;
Config.BaseUrl = TEXT("https://pb.example.com");
Config.ProfileName = TEXT("production");

FOpenPocketBaseClientResult Result = FOpenPocketBaseClient::Create(Config);
if (!Result.IsSuccess())
{
    ReportSetupError(Result.GetError());
    return;
}

PocketBase = Result.TakeValue();`,
            "MyGameInstance.cpp"
          ),
          paragraph(
            "Create validates the origin and policy before returning a client. The shared thread-safe reference is the lifetime boundary. Collection and file service values are lightweight views over that client, not separate connections."
          ),
        ],
      },
      {
        id: "results",
        title: "A result contains a value or an error",
        blocks: [
          paragraph(
            "Every native completion receives TOpenPocketBaseResult<T>. Check IsSuccess before GetValue or TakeValue. Errors share the same shape across HTTP, validation, parsing, storage, capability, and cancellation failures, which keeps the UI and logging path consistent."
          ),
          code(
            "cpp",
            `Client->Health([](TOpenPocketBaseResult<FOpenPocketBaseHealthResult>&& Result)
{
    if (!Result.IsSuccess())
    {
        PresentError(Result.GetError());
        return;
    }

    const FOpenPocketBaseHealthResult& Health = Result.GetValue();
    UpdateServerBadge(Health.bHealthy, Health.DurationSeconds);
});`
          ),
        ],
      },
      {
        id: "handles",
        title: "Request handles are for control, not completion",
        blocks: [
          paragraph(
            "A request keeps running if its handle is destroyed. Retain FOpenPocketBaseRequestHandle only when the owner might cancel. Calling Cancel is idempotent and results in one cancellation completion. Shutdown cancels all active work and ignores late transport callbacks."
          ),
          code("cpp", `FOpenPocketBaseRequestHandle PendingRequest = Client->Health(MoveTemp(Callback));

PendingRequest.Cancel();
Client->Shutdown();`),
          callout(
            "warning",
            "Protect UObject captures",
            "Callbacks run on the game thread, but the object that started a request may already be gone. Capture a TWeakObjectPtr and check IsValid before touching it."
          ),
        ],
      },
      {
        id: "dependencies",
        title: "Inject boundaries in tests",
        blocks: [
          paragraph(
            "FOpenPocketBaseClientDependencies can replace the transport, secure store, clock, and OAuth browser. This is the intended seam for deterministic tests. Production callers normally use the standard Create overload and platform implementations."
          ),
          bullets(
            "A scripted transport verifies request construction and response handling without a live server.",
            "An injected clock makes expiry hints, retry delays, and OAuth transaction deadlines deterministic.",
            "A mock secure store proves persistence failure behavior without touching a device keychain.",
            "An injected OAuth browser keeps provider handoff out of editor automation."
          ),
        ],
      },
    ],
  },
];
