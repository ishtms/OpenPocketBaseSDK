import { bullets, callout, cards, lead, paragraph, screenshot, steps, table, type DocPage } from "./types";

const shot = (
  asset: string,
  title: string,
  text: string,
  caption: string
) => screenshot(
  `[put a screenshot of ${text} here]`,
  { asset: `/tutorials/${asset}`, title, caption }
);

export const tutorialPages: DocPage[] = [
  {
    slug: "tutorials/getting-started",
    title: "Build your tutorial project",
    eyebrow: "Blueprint course / 01",
    description: "Prepare one clean Unreal project, one small PocketBase collection, and a schema-aware profile that every tutorial can build on.",
    readTime: "10 min",
    tutorial: {
      order: 1,
      level: "Beginner",
      outcome: "A reusable tutorial project with the plugin enabled, PocketBase running, and typed collection pickers ready.",
      prerequisites: ["Unreal Engine 5.8", "PocketBase v0.39.11", "An empty Third Person project"],
    },
    sections: [
      {
        id: "what-you-build",
        title: "What you are building",
        blocks: [
          lead("This course builds a tiny task board entirely in Blueprint. You will connect once, create and query records, log a player in, and subscribe to live changes. Each page leaves you with a working graph before introducing the next idea, then points into the focused file and production guides."),
          table(
            ["PocketBase collection", "Field", "Type", "Required"],
            [
              ["tasks", "title", "Text", "Yes"],
              ["tasks", "done", "Bool", "No"],
              ["tasks", "priority", "Number", "No"],
              ["tasks", "owner", "Relation to users", "No"],
              ["tasks", "attachment", "File", "No"],
            ]
          ),
          callout("note", "Use your own names if you want", "The screenshots and pin labels in this course use tasks and users. Different names are fine, but select them through the schema picker instead of typing strings into every graph."),
        ],
      },
      {
        id: "install-plugin",
        title: "Install and enable the plugin",
        blocks: [
          steps(
            { title: "Close Unreal Editor", text: "Copy OpenPocketBaseSDK into YourProject/Plugins/OpenPocketBaseSDK. The .uplugin file must sit directly inside that folder." },
            { title: "Open the project", text: "Allow Unreal to build missing modules if this is a C++ project, then open Edit > Plugins." },
            { title: "Enable the SDK", text: "Find Open PocketBase SDK under Online, enable it, and restart only if Unreal asks." }
          ),
          shot("01-getting-started/01-plugin-enabled.png", "Capture 01 · Plugin enabled", "the Open PocketBase SDK entry enabled in Unreal's Plugins window", "Crop around the plugin row and keep the Enabled checkbox visible."),
        ],
      },
      {
        id: "prepare-pocketbase",
        title: "Prepare PocketBase",
        blocks: [
          steps(
            { title: "Start PocketBase", text: "Run PocketBase locally and open its dashboard. The examples use http://127.0.0.1:8090." },
            { title: "Create tasks", text: "Add the title, done, priority, owner, and attachment fields from the table above." },
            { title: "Create users", text: "Use an auth collection named users and create one test account you can safely use in PIE." },
            { title: "Set API rules", text: "For a local tutorial, set rules that allow your intended test flow. Do not copy permissive development rules into production." }
          ),
          shot("01-getting-started/02-pocketbase-tasks.png", "Capture 02 · PocketBase schema", "the tasks collection and its five tutorial fields in the PocketBase dashboard", "Show field names and types. Hide emails, tokens, and unrelated collections."),
        ],
      },
      {
        id: "import-schema",
        title: "Import the schema into Unreal",
        blocks: [
          paragraph("Export the PocketBase collections JSON, then drag that JSON file into the Unreal Content Browser. The SDK importer creates a PocketBase Schema asset rather than a Data Table."),
          steps(
            { title: "Create a PocketBase content folder", text: "Keep the imported schema somewhere obvious, for example Content/PocketBase." },
            { title: "Drop in the JSON", text: "Choose the PocketBase Schema importer if Unreal asks, then name the asset PB_TutorialSchema." },
            { title: "Open the asset", text: "Confirm tasks and users appear, then save it." }
          ),
          shot("01-getting-started/03-schema-asset.png", "Capture 03 · Imported schema", "PB_TutorialSchema selected in the Content Browser beside its source JSON", "Keep the Content Browser path and the schema asset icon visible."),
          shot("01-getting-started/04-schema-summary.png", "Capture 04 · Schema summary", "the schema asset editor showing tasks and users with no validation errors", "Show the collection summary and validation state, not the entire editor window."),
        ],
      },
      {
        id: "create-profile",
        title: "Create the Local profile",
        blocks: [
          steps(
            { title: "Open Project Settings", text: "Go to Plugins > Open PocketBase SDK." },
            { title: "Add a profile", text: "Name it Local and set Base URL to http://127.0.0.1:8090." },
            { title: "Assign the schema", text: "Select PB_TutorialSchema so Blueprint nodes receive searchable collection and field pickers." },
            { title: "Make it default", text: "Choose Local as the default profile and leave session persistence at Memory Only for the tutorial." }
          ),
          shot("01-getting-started/05-local-profile.png", "Capture 05 · Local profile", "the complete Local Open PocketBase project profile with PB_TutorialSchema assigned", "Show profile name, Base URL, schema, and persistence policy. Never include credentials."),
          callout("success", "Ready for Blueprint", "Open any Blueprint action menu and search for Initialize PocketBase from Project Settings. If the node appears and the schema picker can see tasks, this tutorial is complete."),
          cards(
            { title: "Next: connection and health", text: "Create the default Game Instance client and prove that PocketBase is reachable.", link: "tutorials/connection-health", label: "TUTORIAL 02" },
            { title: "Installation reference", text: "Need module names, folder layout, or troubleshooting details?", link: "start/installation", label: "REFERENCE" }
          ),
        ],
      },
    ],
  },
  {
    slug: "tutorials/connection-health",
    title: "Connect and check server health",
    eyebrow: "Blueprint course / 02",
    description: "Create the default PocketBase client in your Game Instance, handle initialization cleanly, and send your first real request.",
    readTime: "12 min",
    tutorial: {
      order: 2,
      level: "Beginner",
      outcome: "One durable Game Instance client and a health check with visible success, failure, and cancellation paths.",
      prerequisites: ["Tutorial 01 complete", "Local PocketBase running", "Local profile selected"],
    },
    sections: [
      {
        id: "game-instance",
        title: "Create the client owner",
        blocks: [
          lead("The default client belongs in the Game Instance because it survives level changes. Create it once, then retrieve the same client from gameplay Blueprints instead of initializing a new connection in every Actor."),
          steps(
            { title: "Create BP_TutorialGameInstance", text: "Create a Blueprint class derived from GameInstance and open its Event Graph." },
            { title: "Assign it", text: "Open Project Settings > Maps & Modes and select BP_TutorialGameInstance as Game Instance Class." },
            { title: "Use Event Init", text: "Add Initialize PocketBase from Project Settings after Event Init and leave Profile Name empty to use Local." }
          ),
          shot("02-connection-health/01-game-instance-class.png", "Capture 01 · Game Instance class", "BP_TutorialGameInstance assigned under Maps & Modes", "Crop to the Game Instance Class setting and its selected value."),
          shot("02-connection-health/02-initialize-node.png", "Capture 02 · Initialize node", "Event Init connected to Initialize PocketBase from Project Settings in BP_TutorialGameInstance", "Include the node title, profile input, and all terminal outputs."),
        ],
      },
      {
        id: "handle-initialization",
        title: "Handle initialization once",
        blocks: [
          steps(
            { title: "Use Succeeded", text: "Print PocketBase initialized during this tutorial. Later this output can start your loading flow." },
            { title: "Use Failed", text: "Connect the structured Error to Print String. The SDK's safe Blueprint conversion keeps sensitive values redacted." },
            { title: "Do not cache a random Actor reference", text: "The Game Instance already owns the default client. Other Blueprints can retrieve it when needed." }
          ),
          shot("02-connection-health/03-init-terminal-paths.png", "Capture 03 · Initialization paths", "the initialization Succeeded and Failed outputs connected to separate Print String nodes", "Keep the Error data wire visible on the failure path."),
        ],
      },
      {
        id: "health-request",
        title: "Send the health request",
        blocks: [
          steps(
            { title: "Continue from Succeeded", text: "Add Check Health and connect the initialized Client value to Pocket Base Client." },
            { title: "Print Success", text: "Connect the returned Health Result directly to Print String." },
            { title: "Print Failed", text: "Connect Error directly to a second Print String." },
            { title: "Handle Cancelled", text: "Give cancellation its own Print String so every terminal path is visible while learning." }
          ),
          shot("02-connection-health/04-health-unwired.png", "Capture 04 · Health nodes placed", "Initialize PocketBase and Check Health placed before their execution and data pins are connected", "This capture should make each node and pin name readable."),
          shot("02-connection-health/05-health-complete.png", "Capture 05 · Finished health graph", "the complete Game Instance Init graph with initialization and health terminal paths", "Capture only the final graph area, with straight readable wires and no unrelated nodes."),
          callout("success", "Expected result", "With PocketBase running, Check Health fires Success exactly once. Stop PocketBase and try again: Failed should fire with a Transport error, without an Accessed None warning or editor crash."),
        ],
      },
      {
        id: "retrieve-client",
        title: "Retrieve the client elsewhere",
        blocks: [
          paragraph("In an Actor, Widget, Player Controller, or subsystem, use Get PocketBase Client. Its True path returns the same default client created by the Game Instance. Use False as a real startup-state branch rather than dereferencing an unset value."),
          shot("02-connection-health/06-get-client.png", "Capture 06 · Retrieve the client", "Get PocketBase Client branching through True and False in a gameplay Blueprint", "Show the Client output being consumed only from the True execution path."),
          cards(
            { title: "Next: create a record", text: "Build an immutable request body and create the first task.", link: "tutorials/create-record", label: "TUTORIAL 03" },
            { title: "Request lifecycle reference", text: "Understand cancellation, timeouts, retries, and structured errors.", link: "core/requests-errors", label: "DEEP DIVE" }
          ),
        ],
      },
    ],
  },
  {
    slug: "tutorials/create-record",
    title: "Create your first record",
    eyebrow: "Blueprint course / 03",
    description: "Use schema-aware field pickers and pure body builders to create a task without writing JSON or collection-name strings.",
    readTime: "12 min",
    tutorial: {
      order: 3,
      level: "Beginner",
      outcome: "A Create Record graph that returns and stores the new task ID.",
      prerequisites: ["Tutorial 02 complete", "tasks collection imported", "Create rule permits the tutorial request"],
    },
    sections: [
      {
        id: "collection",
        title: "Choose the collection",
        blocks: [
          steps(
            { title: "Create BP_CreateTask", text: "Use an Actor for this isolated lesson and place one instance in the level." },
            { title: "Get the client", text: "On an input event, call Get PocketBase Client and continue only from True." },
            { title: "Use Collection", text: "Drag from Client, add Use Collection, and select tasks through the Reference picker." }
          ),
          shot("03-create-record/01-client-collection.png", "Capture 01 · Client and collection", "Get PocketBase Client connected to Use Collection with tasks selected", "Keep the schema-backed tasks label readable."),
        ],
      },
      {
        id: "build-body",
        title: "Build the record body",
        blocks: [
          paragraph("Record-body builders are pure. Each With Field node returns a new body, so the wire itself shows exactly which values will be sent."),
          steps(
            { title: "Add New Record Body", text: "This is the empty mutation value." },
            { title: "Add With String Field", text: "Select tasks.title and set Value to Learn OpenPocketBase." },
            { title: "Add With Boolean Field", text: "Select tasks.done and leave Value false." },
            { title: "Add With Number Field", text: "Select tasks.priority and set Value to 1." }
          ),
          shot("03-create-record/02-empty-body.png", "Capture 02 · Empty body", "New Record Body and the three With Field nodes placed but not yet connected", "Capture pin labels before wires cover them."),
          shot("03-create-record/03-body-chain.png", "Capture 03 · Body builder chain", "New Record Body chained through title, done, and priority field builders", "Make the left-to-right immutable body flow obvious."),
        ],
      },
      {
        id: "send-create",
        title: "Send Create Record",
        blocks: [
          steps(
            { title: "Add Create Record", text: "Connect Use Collection's value to Collection and the final builder output to Body." },
            { title: "Store the ID", text: "On Success, break or read the returned Record and save Id to a variable named CreatedTaskId." },
            { title: "Handle errors", text: "Print Error on Failed and give Cancelled its own path." }
          ),
          shot("03-create-record/04-create-node.png", "Capture 04 · Create request", "the tasks collection and completed body connected to Create Record", "Show the async node's Success, Failed, and Cancelled outputs."),
          shot("03-create-record/05-create-complete.png", "Capture 05 · Finished create graph", "the complete create task graph storing CreatedTaskId and printing failures", "The returned Record to Id path must be readable."),
          callout("success", "Verify in both places", "Play once, confirm Success fires, then open PocketBase and verify the new row contains title, done, and priority. Keep the CreatedTaskId for the next tutorial."),
          cards({ title: "Next: read records", text: "Fetch the task by ID, list tasks, and read dynamic fields safely.", link: "tutorials/read-records", label: "TUTORIAL 04" }),
        ],
      },
    ],
  },
  {
    slug: "tutorials/read-records",
    title: "Read one record and list many",
    eyebrow: "Blueprint course / 04",
    description: "Fetch the task you created, inspect its fields safely, and render a page of records without unsafe array assumptions.",
    readTime: "14 min",
    tutorial: {
      order: 4,
      level: "Beginner",
      outcome: "A safe Get Record path and a List Records loop that never indexes an empty page.",
      prerequisites: ["Tutorial 03 complete", "A valid task ID", "List and view rules permit the request"],
    },
    sections: [
      {
        id: "get-one",
        title: "Fetch one task by ID",
        blocks: [
          steps(
            { title: "Get the default client", text: "Retrieve the client and continue from True." },
            { title: "Select tasks", text: "Use Collection and choose tasks from the schema picker." },
            { title: "Add Get Record", text: "Connect CreatedTaskId to Record Id and the collection value to Collection." },
            { title: "Handle all terminals", text: "Use Success for field reads, print Failed, and keep a Cancelled path." }
          ),
          shot("04-read-records/01-get-record.png", "Capture 01 · Get one record", "Get PocketBase Client and tasks connected to Get Record with a Record Id variable", "Show the Record Id wire and all terminal outputs."),
        ],
      },
      {
        id: "read-fields",
        title: "Read fields without guessing",
        blocks: [
          paragraph("PocketBase record data is dynamic. Use the schema-aware Try Get nodes so missing, null, and wrong-type values stay observable instead of silently becoming misleading defaults."),
          steps(
            { title: "Read title", text: "Pass Record into Try Get String Field and select tasks.title." },
            { title: "Read done", text: "Use Try Get Boolean Field with tasks.done." },
            { title: "Branch on success", text: "Only send each returned value to UI from the successful field-read path." }
          ),
          shot("04-read-records/02-field-readers.png", "Capture 02 · Safe field reads", "the returned Record feeding schema-aware title and done field reader nodes", "Keep the success booleans or field-state outputs visible."),
          shot("04-read-records/03-field-output.png", "Capture 03 · Display values", "successful title and done reads updating simple UI text or Print String nodes", "Show that value consumers run only after a successful read."),
        ],
      },
      {
        id: "list-page",
        title: "List a page safely",
        blocks: [
          steps(
            { title: "Add List Records", text: "Use the tasks collection and create List Options with Page 1 and Per Page 10." },
            { title: "Break the page", text: "Read Items, Page, Per Page, and optional total metadata from the successful result." },
            { title: "Use For Each Loop", text: "Iterate Items. Do not Get index 0 or 1 unless you have checked the array length." },
            { title: "Read each title", text: "Inside the loop, use Try Get String Field on the Array Element." }
          ),
          shot("04-read-records/04-list-page.png", "Capture 04 · List one page", "tasks connected to List Records with explicit page and per-page options", "Keep Page 1 and Per Page 10 visible."),
          shot("04-read-records/05-safe-loop.png", "Capture 05 · Safe record loop", "List Records Items feeding For Each Loop and Try Get String Field", "The graph should contain no fixed array index node."),
          callout("warning", "Empty is a valid result", "A successful page can contain zero items. Treat that as an empty state, not an error and not permission to read index zero."),
          cards({ title: "Next: update a record", text: "Send only the fields that changed and confirm the returned record.", link: "tutorials/update-record", label: "TUTORIAL 05" }),
        ],
      },
    ],
  },
  {
    slug: "tutorials/update-record",
    title: "Update an existing record",
    eyebrow: "Blueprint course / 05",
    description: "Build a small patch body, update the task by ID, and use the returned server record as the new source of truth.",
    readTime: "9 min",
    tutorial: {
      order: 5,
      level: "Beginner",
      outcome: "A task update that changes done and priority without resending unrelated fields.",
      prerequisites: ["Tutorial 04 complete", "A valid task ID", "Update rule permits the request"],
    },
    sections: [
      {
        id: "patch-body",
        title: "Build only the changed fields",
        blocks: [
          lead("Update bodies are patches. Start with a fresh body and add only the fields your current interaction is responsible for changing."),
          steps(
            { title: "Create a new body", text: "Use New Record Body rather than reusing the body from Create." },
            { title: "Set done", text: "Add With Boolean Field, select tasks.done, and set Value true." },
            { title: "Raise priority", text: "Add With Number Field, select tasks.priority, and set Value 2." }
          ),
          shot("05-update-record/01-update-body.png", "Capture 01 · Patch body", "a new record body containing only done and priority", "Do not include title in this update graph."),
        ],
      },
      {
        id: "send-update",
        title: "Update by record ID",
        blocks: [
          steps(
            { title: "Add Update Record", text: "Connect tasks, CreatedTaskId, and the patch body." },
            { title: "Use returned Record", text: "On Success, read done and priority from the returned record instead of assuming the server accepted your local values." },
            { title: "Keep terminal paths separate", text: "Failed receives the structured error. Cancelled is an intentional terminal result, not a failure." }
          ),
          shot("05-update-record/02-update-request.png", "Capture 02 · Update request", "tasks, CreatedTaskId, and the patch body connected to Update Record", "Show all inputs and terminal execution pins."),
          shot("05-update-record/03-updated-values.png", "Capture 03 · Confirm returned values", "the successful updated Record feeding safe done and priority field reads", "Keep the server-returned Record wire visible."),
          shot("05-update-record/04-update-complete.png", "Capture 04 · Finished update graph", "the complete update flow including error and cancellation handling", "Capture the smallest graph region that still shows the whole flow."),
          cards({ title: "Next: delete a record", text: "Delete the tutorial task and update local UI only after success.", link: "tutorials/delete-record", label: "TUTORIAL 06" }),
        ],
      },
    ],
  },
  {
    slug: "tutorials/delete-record",
    title: "Delete a record safely",
    eyebrow: "Blueprint course / 06",
    description: "Delete the tutorial task, handle a missing record cleanly, and avoid removing local UI before the server confirms success.",
    readTime: "8 min",
    tutorial: {
      order: 6,
      level: "Beginner",
      outcome: "A deletion flow whose local state changes only after PocketBase confirms the record is gone.",
      prerequisites: ["Tutorial 05 complete", "A valid task ID", "Delete rule permits the request"],
    },
    sections: [
      {
        id: "delete-request",
        title: "Send Delete Record",
        blocks: [
          steps(
            { title: "Retrieve the client", text: "Continue from Get PocketBase Client's True output." },
            { title: "Select tasks", text: "Use Collection with the schema-backed tasks reference." },
            { title: "Add Delete Record", text: "Connect CreatedTaskId to Record Id." },
            { title: "Clear local state on Success", text: "Only now clear CreatedTaskId or remove the matching widget row." }
          ),
          shot("06-delete-record/01-delete-request.png", "Capture 01 · Delete request", "tasks and CreatedTaskId connected to Delete Record", "Include Success, Failed, and Cancelled."),
          shot("06-delete-record/02-success-state.png", "Capture 02 · Success updates local state", "Delete Record Success clearing CreatedTaskId and removing the task row", "Show the local state mutation beginning from Success, not before the request."),
        ],
      },
      {
        id: "missing-record",
        title: "Handle an already-missing record",
        blocks: [
          paragraph("Run the same delete again. PocketBase should reject the missing ID, and the SDK should deliver a structured failure. Your graph must remain valid and your UI should already be in its empty state."),
          steps(
            { title: "Print the failure", text: "Connect Error directly to Print String during the tutorial." },
            { title: "Do not retry blindly", text: "Delete is a mutation. Resolve whether the local state is already correct instead of replaying ambiguous work automatically." }
          ),
          shot("06-delete-record/03-delete-error.png", "Capture 03 · Missing-record failure", "Delete Record Failed connected to a structured error display", "Keep the Error value and the absence of a retry loop visible."),
          shot("06-delete-record/04-delete-complete.png", "Capture 04 · Finished delete graph", "the complete delete flow with server-confirmed local cleanup", "Capture the final readable graph after reroute cleanup."),
          callout("success", "CRUD complete", "You have now created, read, updated, and deleted PocketBase records entirely from Blueprint. The next tutorial turns list options into a real query."),
          cards({ title: "Next: filter, sort, and page", text: "Build a typed query and render pages without unsafe indexing.", link: "tutorials/query-records", label: "TUTORIAL 07" }),
        ],
      },
    ],
  },
  {
    slug: "tutorials/query-records",
    title: "Filter, sort, and page records",
    eyebrow: "Blueprint course / 07",
    description: "Build a typed done=false filter, request a stable sort order, and move between PocketBase pages intentionally.",
    readTime: "13 min",
    tutorial: {
      order: 7,
      level: "Intermediate",
      outcome: "A reusable paged task query with typed filters, deterministic sorting, and an empty state.",
      prerequisites: ["CRUD tutorials complete", "Several tasks in PocketBase", "tasks schema available"],
    },
    sections: [
      {
        id: "typed-filter",
        title: "Build the filter",
        blocks: [
          steps(
            { title: "Add Boolean Filter", text: "Select tasks.done, choose Equals, and leave Value false." },
            { title: "Keep the typed value", text: "Connect the filter result directly to List Options. Do not rebuild done = false as a string." },
            { title: "Inspect validity", text: "If you compose dynamic filters later, stop locally when a builder reports an invalid filter." }
          ),
          shot("07-query-records/01-boolean-filter.png", "Capture 01 · Typed Boolean filter", "a schema-backed Boolean Filter for tasks.done equals false", "Show the selected field, comparison, and Boolean value."),
        ],
      },
      {
        id: "list-options",
        title: "Create deterministic list options",
        blocks: [
          steps(
            { title: "Set Page and Per Page", text: "Use Page 1 and Per Page 5 for a visible tutorial result." },
            { title: "Add sort entries", text: "Sort priority descending, then created descending so equal priorities have a stable order." },
            { title: "Connect Filter", text: "Use the typed filter output from the previous step." },
            { title: "Leave totals enabled", text: "For this screen, totals make Page X of Y possible. Skip them only when the UI does not need them." }
          ),
          shot("07-query-records/02-list-options.png", "Capture 02 · Query options", "List Options with page, per-page, typed filter, and two sort entries", "Expand only the relevant struct pins so the capture stays readable."),
          shot("07-query-records/03-list-request.png", "Capture 03 · Send the query", "the tasks collection and query options connected to List Records", "Include the page result and all terminal paths."),
        ],
      },
      {
        id: "render-page",
        title: "Render the page and empty state",
        blocks: [
          steps(
            { title: "Break Record Page", text: "Use Items, Page, Per Page, Total Items, and Total Pages from Success." },
            { title: "Check Items length", text: "Branch to an empty-state message when zero, otherwise use For Each Loop." },
            { title: "Advance deliberately", text: "When Next is pressed, increment a CurrentPage variable and send a new request with the same filter and sort." },
            { title: "Replace stale UI queries", text: "Give screen-refresh requests the same Request Key with replacement enabled so an older page cannot overwrite a newer one." }
          ),
          shot("07-query-records/04-page-render.png", "Capture 04 · Page and empty state", "Record Page Items branching between an empty state and a For Each Loop", "The length check must be visible before any element access."),
          shot("07-query-records/05-next-page.png", "Capture 05 · Next-page request", "a Next action incrementing CurrentPage and rebuilding the same List Options", "Show the shared filter, sort, and request replacement key."),
          cards(
            { title: "Next: authenticate a player", text: "Log in through an auth collection and react to ordered session changes.", link: "tutorials/authentication", label: "TUTORIAL 08" },
            { title: "Filter reference", text: "See every typed comparison and composition rule.", link: "records/filters", label: "DEEP DIVE" }
          ),
        ],
      },
    ],
  },
  {
    slug: "tutorials/authentication",
    title: "Log in and manage the session",
    eyebrow: "Blueprint course / 08",
    description: "Authenticate a PocketBase user, read the safe session snapshot, respond to session changes, and log out cleanly.",
    readTime: "15 min",
    tutorial: {
      order: 8,
      level: "Intermediate",
      outcome: "A Blueprint login flow with no token handling and one central session-driven UI state.",
      prerequisites: ["Tutorial 02 complete", "users auth collection imported", "One test user"],
    },
    sections: [
      {
        id: "auth-collection",
        title: "Choose the auth collection",
        blocks: [
          steps(
            { title: "Retrieve the client", text: "Use Get PocketBase Client and continue from True." },
            { title: "Use Collection", text: "Connect the client and select users. The downstream auth node constrains the picker to compatible auth collections." },
            { title: "Add Log In with Password", text: "Connect the collection, then supply the identity and password from your temporary tutorial UI." }
          ),
          shot("08-authentication/01-auth-collection.png", "Capture 01 · Auth collection", "Use Collection selecting users for Log In with Password", "Show that users comes from the schema picker rather than a string."),
          shot("08-authentication/02-login-node.png", "Capture 02 · Login node", "Log In with Password with Authenticated, Mfa Required, Failed, and Cancelled outputs", "Hide the real password value before capturing."),
        ],
      },
      {
        id: "login-result",
        title: "Handle the authentication result",
        blocks: [
          steps(
            { title: "Use Authenticated", text: "Read the returned authentication result and update the UI from its safe record data." },
            { title: "Keep MFA separate", text: "Mfa Required is a continuation, not a generic failure. This tutorial can display a message and stop there." },
            { title: "Display Failed safely", text: "Use the structured Error. Never print or store a raw auth token." }
          ),
          shot("08-authentication/03-login-terminals.png", "Capture 03 · Login terminal paths", "all four login execution outputs connected to explicit handlers", "Keep credential input values redacted or disconnected."),
          shot("08-authentication/04-current-session.png", "Capture 04 · Safe session snapshot", "Get Current Session reading authenticated state and the auth record without exposing a token", "Show the True and False execution branches."),
        ],
      },
      {
        id: "session-events",
        title: "Drive UI from session changes",
        blocks: [
          paragraph("Bind once to Session Changed on the client owner. Treat the snapshot as the single place that decides whether account UI, player name, and signed-in actions are visible."),
          steps(
            { title: "Bind after initialization", text: "Use Bind Event to Session Changed from the initialized client." },
            { title: "Create a custom event", text: "Name it OnPocketBaseSessionChanged and break the snapshot or reason only where needed." },
            { title: "Add Logout", text: "Call Logout on the client. The local session clears immediately and the same event updates UI." }
          ),
          shot("08-authentication/05-session-event.png", "Capture 05 · Session Changed binding", "Bind Event to Session Changed connected to a named custom event", "Capture the binding beside the initialization flow."),
          shot("08-authentication/06-logout.png", "Capture 06 · Logout flow", "Logout causing the session event to return the UI to a signed-out state", "Show one session-driven UI path rather than duplicate manual cleanup."),
          callout("warning", "The token is not gameplay data", "The SDK owns token storage, refresh, and redaction. Gameplay Blueprints should use the auth record and safe session snapshot, never extract or log the credential."),
          cards({ title: "Next: realtime updates", text: "Subscribe to tasks, keep the subscription alive, and render create, update, and delete events.", link: "tutorials/realtime", label: "TUTORIAL 09" }),
        ],
      },
    ],
  },
  {
    slug: "tutorials/realtime",
    title: "Show task changes in realtime",
    eyebrow: "Blueprint course / 09",
    description: "Open a tasks subscription, store its lifetime handle, process typed events, and shut it down without exit-PIE errors.",
    readTime: "16 min",
    tutorial: {
      order: 9,
      level: "Intermediate",
      outcome: "A live task board that responds to creates, updates, and deletes while owning its subscription correctly.",
      prerequisites: ["Tutorial 07 complete", "PocketBase realtime enabled", "A Blueprint with a clear lifetime"],
    },
    sections: [
      {
        id: "subscribe",
        title: "Create and retain the subscription",
        blocks: [
          lead("A subscription is a lifetime handle, not a fire-and-forget request. Store it on the Blueprint that owns the live screen, and unsubscribe when that owner is done."),
          steps(
            { title: "Get the client", text: "Continue only when Get PocketBase Client succeeds." },
            { title: "Select tasks", text: "Use Collection with the typed tasks reference." },
            { title: "Subscribe to Records", text: "Optionally reuse the done=false filter from Tutorial 07." },
            { title: "Promote the result", text: "Store the returned subscription as TaskSubscription on the owning Actor or Widget." }
          ),
          shot("09-realtime/01-subscribe-node.png", "Capture 01 · Subscribe node", "tasks connected to Subscribe to Records with its subscription result visible", "Show the collection and optional filter inputs."),
          shot("09-realtime/02-store-subscription.png", "Capture 02 · Retain the handle", "the successful subscription promoted to a TaskSubscription variable", "The variable owner and assignment should be clear."),
        ],
      },
      {
        id: "bind-events",
        title: "Bind state, data, and error events",
        blocks: [
          steps(
            { title: "Bind connection state", text: "Show Connecting, Active, Reconnecting, and Stopped in a small status label." },
            { title: "Bind record events", text: "Use the typed create, update, and delete action or event data to update the matching task row." },
            { title: "Bind errors", text: "Display a recoverable status without destroying the subscription owner." },
            { title: "Handle resync", text: "When the SDK signals a possible gap, rerun the bounded list query from Tutorial 07." }
          ),
          shot("09-realtime/03-bind-delegates.png", "Capture 03 · Subscription delegates", "TaskSubscription binding connection, event, error, and resync handlers", "Arrange the four bindings vertically so their names remain readable."),
          shot("09-realtime/04-record-event.png", "Capture 04 · Apply a record event", "a realtime task event choosing create, update, or delete behavior", "Show record Id matching before modifying an existing UI row."),
          shot("09-realtime/05-resync.png", "Capture 05 · Resync query", "the resync signal triggering the same bounded task list query used for initial load", "Make the shared query function or event obvious."),
        ],
      },
      {
        id: "teardown",
        title: "Stop cleanly",
        blocks: [
          steps(
            { title: "Choose the owner", text: "A Widget owns a screen subscription; an Actor owns a level interaction subscription; the Game Instance owns only truly global subscriptions." },
            { title: "Unsubscribe", text: "On Destruct or EndPlay, check TaskSubscription and call Unsubscribe." },
            { title: "Clear the variable", text: "Release the handle after unsubscribe so repeated screen opens create one fresh subscription." }
          ),
          shot("09-realtime/06-clean-teardown.png", "Capture 06 · Clean teardown", "Widget Destruct or Actor EndPlay unsubscribing TaskSubscription before clearing it", "Show the validity guard and the exact owner lifecycle event."),
          callout("success", "Blueprint course complete", "Open PocketBase in another window and create, update, then delete a task. The running PIE screen should react to each change and exit without Accessed None errors."),
          cards(
            { title: "Upload and download files", text: "Continue into the focused file guides when your records need attachments.", link: "files/uploads", label: "NEXT GUIDE" },
            { title: "Realtime reference", text: "Read reconnect, gap, topic, and ownership details.", link: "realtime/subscriptions", label: "DEEP DIVE" },
            { title: "Blueprint node index", text: "Find every gameplay-facing node by workflow.", link: "reference/blueprint-nodes", label: "REFERENCE" }
          ),
        ],
      },
    ],
  },
];
