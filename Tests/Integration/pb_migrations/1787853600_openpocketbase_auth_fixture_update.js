/// <reference path="../pb_data/types.d.ts" />

migrate((app) => {
  const users = app.findCollectionByNameOrId("sdk_users");
  users.passwordAuth.enabled = true;
  users.passwordAuth.identityFields = ["email"];
  users.otp.enabled = true;
  users.otp.duration = 300;
  users.mfa.enabled = true;
  users.mfa.duration = 600;
  users.mfa.rule = "email = 'delivered+mfa-openpocketbase@resend.dev'";
  users.oauth2.enabled = true;
  users.oauth2.providers = [
    {
      name: "github",
      clientId: "openpocketbase-fixture-client",
      clientSecret: "openpocketbase-fixture-secret",
    },
  ];
  app.save(users);

  function resetFixtureRecord(id, email, password) {
    let conflictingRecord = null;
    try {
      conflictingRecord = app.findAuthRecordByEmail("sdk_users", email);
    } catch (_) {
    }
    if (conflictingRecord && conflictingRecord.id !== id) {
      app.delete(conflictingRecord);
    }

    let record;
    try {
      record = app.findRecordById(users, id);
    } catch (_) {
      record = new Record(users);
      record.set("id", id);
    }
    record.set("email", email);
    record.set("emailVisibility", true);
    record.set("verified", true);
    record.set("password", password);
    record.refreshTokenKey();
    app.save(record);
    app.deleteAllExternalAuthsByRecord(record);
    app.deleteAllMFAsByRecord(record);
    app.deleteAllOTPsByRecord(record);
    return record;
  }

  const player = resetFixtureRecord(
    "user00000000001",
    "player@example.com",
    "correct-horse-battery"
  );
  resetFixtureRecord(
    "otpfixture00001",
    "delivered+openpocketbase@resend.dev",
    "otp-fixture-password"
  );
  resetFixtureRecord(
    "mfafixture00001",
    "delivered+mfa-openpocketbase@resend.dev",
    "mfa-fixture-password"
  );

  const interruptedRegistrations = app.findRecordsByFilter(
    users,
    "email ~ {:prefix} && email ~ {:domain}",
    "",
    0,
    0,
    {
      prefix: "chunk28+",
      domain: "@openpocketbase.test",
    }
  );
  for (let recordIndex = 0; recordIndex < interruptedRegistrations.length; recordIndex += 1) {
    app.delete(interruptedRegistrations[recordIndex]);
  }

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
});
