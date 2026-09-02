import { bullets, callout, cards, code, lead, paragraph, screenshot, steps, table, type DocPage } from "./types";

export const startPages: DocPage[] = [
  {
    slug: "overview",
    title: "PocketBase for Unreal, without the glue code",
    eyebrow: "OpenPocketBase SDK",
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
            { title: "Blueprint course", text: "Build a working integration from connection and health through CRUD, authentication, queries, and realtime.", link: "tutorials/getting-started", label: "START HERE" },
            { title: "Records", text: "Create, read, update, delete, filter, expand, project, paginate, and transact.", link: "records/crud", label: "DATA" },
            { title: "Authentication", text: "Password, OTP, MFA continuation, OAuth2, account actions, refresh, and logout.", link: "tutorials/authentication", label: "IDENTITY" },
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
              ["Streaming and realtime", "Platform-gated", "Incremental streaming is packaged-proven on Mac ARM64 and Windows x64. Full realtime manager proof currently covers Mac ARM64."],
              ["Privileged API", "Development only", "The complete admin module is excluded from Shipping by default."],
              ["Offline outbox", "Not included", "The offline module is a reserved extension boundary and exposes no public outbox API in this release."]
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
          screenshot("[put a screenshot of the finished OpenPocketBase tutorial project running in PIE here]"),
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
              ["OpenPocketBaseSDKOffline", "Reserved for a future offline extension. It exposes no public outbox API in this release.", "Optional"],
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
