import { bullets, callout, code, paragraph, screenshot, steps, table, type DocPage } from "./types";

export const dataPages: DocPage[] = [
  {
    slug: "records/crud",
    title: "Create, read, update, and delete records",
    eyebrow: "Records",
    description: "Build request bodies separately from response records, use projections deliberately, and keep mutation outcomes honest.",
    readTime: "12 min",
    sections: [
      {
        id: "record-shapes",
        title: "Bodies and records are different types",
        blocks: [
          paragraph(
            "FOpenPocketBaseRecordBody is mutable request data. FOpenPocketBaseRecord is an immutable response shape with typed metadata and dynamic collection data. Keeping them separate prevents a stale server record from being sent back accidentally with IDs, timestamps, or expansions mixed into a mutation."
          ),
          table(
            ["Type", "Contains", "Use"],
            [
              ["FOpenPocketBaseRecordBody", "Only fields you intend to send", "Create, update, upsert, multipart mutation"],
              ["FOpenPocketBaseRecord", "Id, collection metadata, created, updated, Data, Expanded", "Reading a server response"],
              ["FOpenPocketBaseRecordOptions", "Expand, Fields, request policy", "Shape a single-record response"]
            ]
          ),
          callout("note", "Auth registration uses Create", "Creating a record in an auth collection registers the account when the body includes that collection's required auth fields."),
        ],
      },
      {
        id: "build-body",
        title: "Build a mutation body",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseRecordBody Body;
Body
    .SetStringField(TEXT("title"), TEXT("Ship record CRUD"))
    .SetNumberField(TEXT("score"), 2.0, EOpenPocketBaseFieldModifier::Append)
    .SetStringArrayField(
        TEXT("labels"),
        {TEXT("urgent")},
        EOpenPocketBaseFieldModifier::Prepend);`
          ),
          paragraph(
            "Blueprint uses New Record Body followed by With String Field, With Number Field, With Boolean Field, With Null Field, or With String Array Field. Each pure node returns a new body value."
          ),
          table(
            ["Modifier", "Wire intent", "Typical use"],
            [
              ["Replace", "field", "Set the complete new value"],
              ["Append", "field+", "Add a number, relation, select item, or file where PocketBase supports it"],
              ["Prepend", "+field", "Insert supported list-like values before existing values"],
              ["Remove", "field-", "Remove supported values without replacing the full field"]
            ]
          ),
          screenshot("[put a screenshot of New Record Body -> With String Field -> With Boolean Field here]"),
        ],
      },
      {
        id: "operations",
        title: "Send CRUD operations",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseCollectionService Tasks = Client->Collection(TEXT("tasks"));

Tasks.Create(Body, OnCreated, RecordOptions);
Tasks.GetOne(TEXT("task00000000001"), OnRead, RecordOptions);
Tasks.Update(TEXT("task00000000001"), Body, OnUpdated, RecordOptions);
Tasks.Delete(TEXT("task00000000001"), OnDeleted);`
          ),
          bullets(
            "Create returns the server-created record.",
            "GetOne requires a valid collection and record ID.",
            "Update sends only the fields in the body and returns the updated record.",
            "Delete accepts PocketBase's empty 204 response as success and returns true.",
            "Mutations are never automatically retried after a response could have been lost."
          ),
          screenshot("[put a screenshot of Collection(tasks) connected to Create Record, Update Record, and Delete Record here]"),
        ],
      },
      {
        id: "projection",
        title: "Expand and project only what you need",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseRecordOptions Options;
Options.Expand = {TEXT("owner.team")};
Options.Fields = {
    TEXT("id"),
    UOpenPocketBaseRecordLibrary::MakeExcerptField(TEXT("title"), 80, true),
    TEXT("expand.owner.*")
};`
          ),
          paragraph(
            "Fields follows PocketBase's wire format. Excerpts belong inside that array, so use Make Excerpt Field instead of looking for a separate excerpt option. Expanded relations and back-relations remain in Record.Expanded as dynamic JSON. Record.Data contains collection fields only."
          ),
          callout("warning", "A projection can omit fields", "Field access can report Missing because the server projection intentionally left a field out. Missing is different from an explicit JSON null."),
        ],
      },
    ],
  },
  {
    slug: "records/reading-fields",
    title: "Read dynamic record fields safely",
    eyebrow: "Records",
    description: "Separate metadata from collection data and distinguish missing, null, wrong-type, and present values in Blueprint and C++.",
    readTime: "8 min",
    sections: [
      {
        id: "record-layout",
        title: "What lives where",
        blocks: [
          table(
            ["Member", "Contains"],
            [
              ["Id", "PocketBase record ID"],
              ["CollectionId", "Stable collection ID returned by PocketBase"],
              ["CollectionName", "Collection name returned by PocketBase"],
              ["Created / Updated", "Parsed UTC timestamps"],
              ["Data", "Dynamic fields declared by the collection plus unknown response fields"],
              ["Expanded", "Dynamic relation expansion tree"]
            ]
          ),
          paragraph(
            "Metadata is lifted out of Data so it cannot collide with a collection field helper. Unknown collection fields stay in Data, which gives newer server schemas forward room without dropping information."
          ),
        ],
      },
      {
        id: "field-state",
        title: "Presence and type are separate questions",
        blocks: [
          table(
            ["State", "Meaning", "Common cause"],
            [
              ["Found", "The field exists and has the requested type.", "Normal read"],
              ["Missing", "The key is absent.", "Projection, schema difference, or optional server field"],
              ["Null", "The key exists with JSON null.", "Cleared optional data"],
              ["WrongType", "The key exists but is not the requested type.", "Schema drift or incorrect helper"]
            ]
          ),
          paragraph(
            "Use Has Field and Is Field Null for explicit branching. String and integer fields also provide state-returning helpers when the UI needs to distinguish all four outcomes. Other typed helpers return false for missing, null, or wrong-type values."
          ),
          screenshot("[put a screenshot of Has Field and Get String Field State feeding a Switch node here]"),
        ],
      },
      {
        id: "helpers",
        title: "Typed helpers",
        blocks: [
          bullets(
            "Try Get String Field for text, select values represented as text, and IDs.",
            "Try Get Integer Field rejects fractional values and values outside int64 range.",
            "Try Get Number Field reads a JSON number as double.",
            "Try Get Boolean Field reads true or false without text coercion.",
            "Try Get Date Field parses a PocketBase date into FDateTime.",
            "Try Get String Array Field reads arrays whose elements are all strings.",
            "Try Get Object Field returns a JSON object wrapper for nested custom data."
          ),
          code(
            "cpp",
            `FString Title;
switch (UOpenPocketBaseRecordLibrary::GetStringFieldState(Record, TEXT("title"), Title))
{
case EOpenPocketBaseFieldState::Found:
    ShowTitle(Title);
    break;
case EOpenPocketBaseFieldState::Null:
    ShowEmptyTitle();
    break;
default:
    ShowSchemaIssue();
    break;
}`
          ),
        ],
      },
      {
        id: "dates",
        title: "PocketBase dates",
        blocks: [
          paragraph(
            "Try Parse PocketBase Date handles the server timestamp format. Format PocketBase Date emits the matching UTC representation for filters and project routes. Prefer FDateTime inside gameplay code and format only at the boundary."
          ),
          screenshot("[put a screenshot of Try Get Date Field formatting a task deadline for UI here]"),
        ],
      },
    ],
  },
  {
    slug: "records/filters",
    title: "Build filters without string concatenation",
    eyebrow: "Records",
    description: "Use typed values, compose expressions, understand every comparison family, and reserve raw filters for trusted advanced input.",
    readTime: "10 min",
    sections: [
      {
        id: "typed",
        title: "Start with typed filters",
        blocks: [
          paragraph(
            "Typed filters escape values for PocketBase and prevent user data from becoming filter syntax. Build one condition at a time, then combine them with And or Or. Field names are still structural input, so keep them in developer-controlled code or a fixed mapping."
          ),
          code(
            "cpp",
            `const FOpenPocketBaseFilter Filter =
    FOpenPocketBaseFilter::String(
        TEXT("owner"),
        EOpenPocketBaseStringComparison::Equals,
        UserId)
    .And(FOpenPocketBaseFilter::Boolean(
        TEXT("done"),
        EOpenPocketBaseBooleanComparison::Equals,
        false))
    .And(FOpenPocketBaseFilter::Date(
        TEXT("updated"),
        EOpenPocketBaseDateComparison::OnOrAfter,
        LastSyncUtc));`
          ),
          screenshot("[put a screenshot of three typed filter nodes combined with And Filters here]"),
        ],
      },
      {
        id: "comparisons",
        title: "Comparison families",
        blocks: [
          table(
            ["Type", "Comparisons"],
            [
              ["String", "Equals, Does Not Equal, Contains, Does Not Contain, Any Equals, Any Does Not Equal, Any Contains, Any Does Not Contain"],
              ["Number", "Equals, Does Not Equal, Greater Than, Greater Than or Equal, Less Than, Less Than or Equal, plus the six matching Any variants"],
              ["Boolean", "Equals, Does Not Equal, Any Equals, Any Does Not Equal"],
              ["Date", "Equals, Does Not Equal, Is After, Is On or After, Is Before, Is On or Before, plus the six matching Any variants"],
              ["Null", "Is Null, Is Not Null, Any Is Null, Any Is Not Null"]
            ]
          ),
          bullets(
            "Number Any variants are Any Equals, Any Does Not Equal, Any Greater Than, Any Greater Than or Equal, Any Less Than, and Any Less Than or Equal.",
            "Date Any variants are Any Equals, Any Does Not Equal, Any Is After, Any Is On or After, Any Is Before, and Any Is On or Before."
          ),
          paragraph(
            "Any comparisons use PocketBase's any-element operators for multi-value fields and back-relations. They do not mean a client-side loop. Choose the ordinary comparison for a scalar field and the Any form only when the server field shape calls for it."
          ),
        ],
      },
      {
        id: "composition",
        title: "Composition preserves grouping",
        blocks: [
          paragraph(
            "And Filters and Or Filters add parentheses so meaning survives longer chains. Build the inner alternatives first when you need status = open AND (priority = high OR owner = me)."
          ),
          steps(
            { title: "Build status", text: "Create one String Filter for status equals open." },
            { title: "Build alternatives", text: "Create priority equals high and owner equals the current user, then combine those two with Or Filters." },
            { title: "Join the groups", text: "Combine status with the alternative group using And Filters." }
          ),
          screenshot("[put a screenshot of status AND (priority OR owner) as grouped Blueprint filter nodes here]"),
        ],
      },
      {
        id: "raw",
        title: "Raw filters are an escape hatch",
        blocks: [
          paragraph(
            "Raw Filter (Advanced) accepts a trusted PocketBase expression for syntax the typed builders do not cover. Do not concatenate player input into it. Native C++ also exposes a parameter binder for advanced placeholder expressions."
          ),
          callout("danger", "Keep data out of expression syntax", "If any value comes from a player, URL, save file, or remote config, use a typed builder or the native binder. Escaping a quote by hand is not a filter security policy."),
        ],
      },
    ],
  },
  {
    slug: "records/pagination",
    title: "List pages and bounded full lists",
    eyebrow: "Records",
    description: "Choose one server page or a bounded traversal, preserve truthful totals, and build continuation filters when a cursor is needed.",
    readTime: "11 min",
    sections: [
      {
        id: "single-page",
        title: "List Records returns one PocketBase page",
        blocks: [
          table(
            ["Option", "Purpose"],
            [
              ["Page", "One-based server page number"],
              ["PerPage", "Requested number of records on that page"],
              ["Filter", "Typed or trusted raw PocketBase filter"],
              ["Sort", "Ordered fields such as -updated,id"],
              ["Expand", "Relation paths to expand"],
              ["Fields", "Projection list"],
              ["bSkipTotal", "Ask PocketBase to skip count work when totals are unnecessary"]
            ]
          ),
          paragraph(
            "When bSkipTotal is true, PocketBase may return -1 for totals. The SDK converts that to absence: bHasTotalItems and bHasTotalPages are false. It does not turn an unknown total into zero."
          ),
        ],
      },
      {
        id: "full-list",
        title: "A full list must have a bound",
        blocks: [
          paragraph(
            "Get Full Record List starts at page 1 and follows pages automatically. At least one of MaxItems or MaxPages must be greater than zero. This requirement makes an accidental unbounded collection download impossible."
          ),
          table(
            ["Option", "List Records", "Get Full Record List"],
            [
              ["Page", "Selects the page", "Ignored, traversal starts at page 1"],
              ["PerPage", "Page size", "Batch size for each automatic request"],
              ["MaxItems", "Unused", "Caps total returned records"],
              ["MaxPages", "Unused", "Caps server pages fetched"]
            ]
          ),
          code(
            "cpp",
            `FOpenPocketBaseFullListOptions Options;
Options.ListOptions.PerPage = 100;
Options.ListOptions.bSkipTotal = true;
Options.ListOptions.Sort = {TEXT("created"), TEXT("id")};
Options.MaxItems = 500;
Options.MaxPages = 10;

Client->Collection(TEXT("tasks")).GetFullList(Options, OnFullList);`
          ),
          screenshot("[put a screenshot of Get Full Record List with Per Page, Max Items, and Max Pages expanded here]"),
        ],
      },
      {
        id: "stop-reasons",
        title: "The result tells you why it stopped",
        blocks: [
          bullets(
            "bReachedEnd means the collection ended before a caller limit stopped it.",
            "bReachedItemLimit means MaxItems trimmed or stopped traversal.",
            "bReachedPageLimit means MaxPages stopped traversal before the end was known.",
            "PagesFetched records actual server pages, including the final short page.",
            "Cancellation stops the active page and prevents the next page from starting."
          ),
          callout("note", "Both bounds can be set", "Traversal stops at collection end or whichever bound is reached first. PerPage 10 with MaxItems 25 fetches three pages and returns exactly 25 records."),
        ],
      },
      {
        id: "continuation",
        title: "PocketBase records are page and filter based",
        blocks: [
          paragraph(
            "PocketBase does not expose a native record cursor endpoint. For caller-owned continuation, use a stable unique sort such as created,id and build a filter from the last seen values. Keep bSkipTotal enabled when counts are unnecessary and retain a page or item bound."
          ),
          callout("warning", "Sort must be stable", "Sorting only by created can skip or repeat records when timestamps match. Add id as a deterministic tie-breaker and carry both values into the continuation filter."),
        ],
      },
    ],
  },
  {
    slug: "records/batches",
    title: "Transactional batches",
    eyebrow: "Records",
    description: "Build an explicit PocketBase transaction, configure server and caller bounds, and handle ambiguous outcomes correctly.",
    readTime: "9 min",
    sections: [
      {
        id: "server",
        title: "Enable batches on the server first",
        blocks: [
          paragraph(
            "PocketBase batches are opt-in. Configure batch.enabled, batch.maxRequests, batch.timeout, and batch.maxBodySize on the server. If support is disabled, the SDK reports Unsupported with server code batch_disabled."
          ),
          callout("warning", "A batch is never implicit", "Ordinary record calls are not gathered into a batch. The caller must create FOpenPocketBaseBatchRequest and send it explicitly."),
        ],
      },
      {
        id: "build",
        title: "Build the transaction",
        blocks: [
          code(
            "cpp",
            `FOpenPocketBaseBatchRequest Batch;
Batch
    .AddCreate(TEXT("tasks"), MoveTemp(CreateBody))
    .AddUpdate(TEXT("tasks"), TEXT("task00000000002"), MoveTemp(UpdateBody))
    .AddUpsert(TEXT("tasks"), MoveTemp(UpsertBody))
    .AddDelete(TEXT("tasks"), TEXT("task00000000003"));

FOpenPocketBaseBatchOptions Options;
Options.MaxOperations = 10;
Options.MaxBodyBytes = 1024 * 1024;

Client->SendBatch(MoveTemp(Batch), OnBatchComplete, Options);`
          ),
          paragraph(
            "Blueprint starts with New Batch and chains With Create, With Update, With Upsert, and With Delete. Those nodes return new values, just like record-body builders. Send Batch takes the completed value plus local operation and body-size limits."
          ),
          screenshot("[put a screenshot of New Batch chained through create, update, and delete into Send Batch here]"),
        ],
      },
      {
        id: "result",
        title: "Read one result per operation",
        blocks: [
          table(
            ["Field", "Meaning"],
            [
              ["Operation", "Create, Update, Upsert, or Delete in request order"],
              ["HttpStatus", "Status for that sub-request"],
              ["bHasRecord", "True when PocketBase returned a record"],
              ["Record", "The created, updated, or upserted record when present"]
            ]
          ),
          paragraph(
            "PocketBase executes the batch as a transaction. A failed operation rolls the server transaction back. The client still never automatically retries a mutation batch because a lost response leaves the transaction outcome unknown."
          ),
          callout("note", "Auth record synchronization still applies", "If a successful batch updates the currently authenticated record, memory, secure persistence, auth generation, and the RecordUpdated session event commit together."),
        ],
      },
    ],
  },
];
