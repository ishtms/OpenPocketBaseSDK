/// <reference path="../pb_data/types.d.ts" />

migrate((app) => {
  const tasks = app.findCollectionByNameOrId("sdk_tasks");

  const relationTarget = new Record(tasks);
  relationTarget.set("id", "fieldtask000001");
  relationTarget.set("title", "Field Lab Relation Target");
  relationTarget.set("done", false);
  relationTarget.set("score", 4800);
  app.save(relationTarget);

  const fieldLab = new Collection({
    type: "base",
    name: "sdk_field_lab",
    listRule: "",
    viewRule: "",
    createRule: "@request.auth.id != ''",
    updateRule: "@request.auth.id != ''",
    deleteRule: "@request.auth.id != ''",
    fields: [
      {
        name: "created",
        type: "autodate",
        onCreate: true,
      },
      {
        name: "updated",
        type: "autodate",
        onCreate: true,
        onUpdate: true,
      },
      {
        name: "plainText",
        type: "text",
        required: true,
        max: 5000,
      },
      {
        name: "editorText",
        type: "editor",
      },
      {
        name: "website",
        type: "url",
      },
      {
        name: "contactEmail",
        type: "email",
      },
      {
        name: "quantity",
        type: "number",
      },
      {
        name: "active",
        type: "bool",
      },
      {
        name: "eventDate",
        type: "date",
      },
      {
        name: "status",
        type: "select",
        values: ["draft", "review", "published"],
        maxSelect: 1,
      },
      {
        name: "tags",
        type: "select",
        values: ["alpha", "beta", "unicode", "long-text"],
        maxSelect: 4,
      },
      {
        name: "metadata",
        type: "json",
      },
      {
        name: "location",
        type: "geoPoint",
      },
      {
        name: "primaryTask",
        type: "relation",
        collectionId: tasks.id,
        maxSelect: 1,
      },
      {
        name: "relatedTasks",
        type: "relation",
        collectionId: tasks.id,
        maxSelect: 3,
      },
      {
        name: "documents",
        type: "file",
        maxSelect: 2,
        maxSize: 5 * 1024 * 1024,
        mimeTypes: ["text/plain"],
      },
    ],
  });
  app.save(fieldLab);
}, (app) => {
  const fieldLab = app.findCollectionByNameOrId("sdk_field_lab");
  app.delete(fieldLab);

  const relationTarget = app.findRecordById("sdk_tasks", "fieldtask000001");
  app.delete(relationTarget);
});
