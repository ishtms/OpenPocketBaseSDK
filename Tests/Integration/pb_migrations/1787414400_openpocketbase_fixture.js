/// <reference path="../pb_data/types.d.ts" />

migrate((app) => {
  const superusers = app.findCollectionByNameOrId("_superusers");
  const fixtureSuperuser = new Record(superusers);
  fixtureSuperuser.set("email", "openpocketbase-fixture@example.com");
  fixtureSuperuser.setRandomPassword();
  app.save(fixtureSuperuser);

  const users = new Collection({
    type: "auth",
    name: "sdk_users",
    listRule: null,
    viewRule: null,
    createRule: "",
    updateRule: "id = @request.auth.id",
    deleteRule: "id = @request.auth.id",
    manageRule: "id = @request.auth.id",
    passwordAuth: {
      enabled: true,
      identityFields: ["email"],
    },
  });
  app.save(users);

  const player = new Record(users);
  player.set("id", "user00000000001");
  player.set("email", "player@example.com");
  player.set("emailVisibility", true);
  player.set("verified", true);
  player.set("password", "correct-horse-battery");
  app.save(player);

  const tasks = new Collection({
    type: "base",
    name: "sdk_tasks",
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
        name: "title",
        type: "text",
        required: true,
        max: 200,
      },
      {
        name: "done",
        type: "bool",
      },
      {
        name: "score",
        type: "number",
      },
    ],
  });
  app.save(tasks);

  const task = new Record(tasks);
  task.set("id", "task00000000001");
  task.set("title", "Ship the Unreal SDK");
  task.set("done", false);
  app.save(task);

  const settings = app.settings();
  settings.batch.enabled = true;
  settings.batch.maxRequests = 10;
  settings.batch.timeout = 5;
  settings.batch.maxBodySize = 1024 * 1024;
  app.save(settings);
}, (app) => {
  const settings = app.settings();
  settings.batch.enabled = false;
  app.save(settings);

  const tasks = app.findCollectionByNameOrId("sdk_tasks");
  app.delete(tasks);

  const users = app.findCollectionByNameOrId("sdk_users");
  app.delete(users);

  try {
    const fixtureSuperuser = app.findAuthRecordByEmail(
      "_superusers",
      "openpocketbase-fixture@example.com",
    );
    app.delete(fixtureSuperuser);
  } catch {
    // The disposable fixture superuser may already have been removed.
  }
});
