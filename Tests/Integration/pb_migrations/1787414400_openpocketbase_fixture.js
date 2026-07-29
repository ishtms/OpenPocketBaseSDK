/// <reference path="../pb_data/types.d.ts" />

migrate((app) => {
  const superusers = app.findCollectionByNameOrId("_superusers");
  const fixtureSuperuser = new Record(superusers);
  const fixtureSuperuserEmail = $os.getenv(
    "OPENPOCKETBASE_FIXTURE_SUPERUSER_EMAIL",
  );
  const fixtureSuperuserPassword = $os.getenv(
    "OPENPOCKETBASE_FIXTURE_SUPERUSER_PASSWORD",
  );
  if (!fixtureSuperuserEmail || fixtureSuperuserPassword.length < 20) {
    throw new Error("Ephemeral fixture superuser material is unavailable.");
  }
  fixtureSuperuser.set("email", fixtureSuperuserEmail);
  fixtureSuperuser.set("password", fixtureSuperuserPassword);
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
    mfa: {
      enabled: true,
      duration: 600,
      rule: "email = 'delivered+mfa-openpocketbase@resend.dev'",
    },
    otp: {
      enabled: true,
      duration: 300,
    },
    oauth2: {
      enabled: true,
      providers: [
        {
          name: "github",
          clientId: "openpocketbase-fixture-client",
          clientSecret: "openpocketbase-fixture-secret",
        },
      ],
    },
  });
  app.save(users);

  const player = new Record(users);
  player.set("id", "user00000000001");
  player.set("email", "player@example.com");
  player.set("emailVisibility", true);
  player.set("verified", true);
  player.set("password", "correct-horse-battery");
  player.refreshTokenKey();
  app.save(player);

  const otpFixture = new Record(users);
  otpFixture.set("id", "otpfixture00001");
  otpFixture.set("email", "delivered+openpocketbase@resend.dev");
  otpFixture.set("emailVisibility", true);
  otpFixture.set("verified", true);
  otpFixture.set("password", "otp-fixture-password");
  otpFixture.refreshTokenKey();
  app.save(otpFixture);

  const mfaFixture = new Record(users);
  mfaFixture.set("id", "mfafixture00001");
  mfaFixture.set("email", "delivered+mfa-openpocketbase@resend.dev");
  mfaFixture.set("emailVisibility", true);
  mfaFixture.set("verified", true);
  mfaFixture.set("password", "mfa-fixture-password");
  mfaFixture.refreshTokenKey();
  app.save(mfaFixture);

  const externalAuths = app.findCollectionByNameOrId("_externalAuths");
  const externalAuthFixtures = [
    ["extauth00000001", "github", "fixture-github-user"],
    ["extauth00000002", "google", "fixture-google-user"],
  ];
  for (let externalAuthIndex = 0; externalAuthIndex < externalAuthFixtures.length; externalAuthIndex += 1) {
    const externalAuthFixture = externalAuthFixtures[externalAuthIndex];
    const externalAuth = new Record(externalAuths);
    externalAuth.set("id", externalAuthFixture[0]);
    externalAuth.set("collectionRef", users.id);
    externalAuth.set("recordRef", player.id);
    externalAuth.set("provider", externalAuthFixture[1]);
    externalAuth.set("providerId", externalAuthFixture[2]);
    app.save(externalAuth);
  }

  const tasks = new Collection({
    type: "base",
    name: "sdk_tasks",
    listRule: "",
    viewRule: "@request.context != 'protectedFile' || @request.auth.id != ''",
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
      {
        name: "attachments",
        type: "file",
        maxSelect: 3,
        maxSize: 5 * 1024 * 1024,
        mimeTypes: ["text/plain"],
        protected: true,
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
      $os.getenv("OPENPOCKETBASE_FIXTURE_SUPERUSER_EMAIL"),
    );
    app.delete(fixtureSuperuser);
  } catch {
    // The disposable fixture superuser may already have been removed.
  }
});
