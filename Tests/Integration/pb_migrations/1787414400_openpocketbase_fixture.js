/// <reference path="../pb_data/types.d.ts" />

migrate((app) => {
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
    createRule: null,
    updateRule: null,
    deleteRule: null,
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
    ],
  });
  app.save(tasks);

  const task = new Record(tasks);
  task.set("id", "task00000000001");
  task.set("title", "Ship the Unreal SDK");
  task.set("done", false);
  app.save(task);
}, (app) => {
  const tasks = app.findCollectionByNameOrId("sdk_tasks");
  app.delete(tasks);

  const users = app.findCollectionByNameOrId("sdk_users");
  app.delete(users);
});
