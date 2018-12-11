CREATE ROLE salt WITH PASSWORD 'salt';
ALTER ROLE "salt" WITH LOGIN;
GRANT CREATE, USAGE ON SCHEMA public TO salt;
CREATE DATABASE salt WITH OWNER salt;

CREATE TABLE "public"."jids" (
   "jid" character varying(20) NOT NULL,
   "load" text NOT NULL,
   CONSTRAINT "jids_pkey" PRIMARY KEY ("jid")
) WITH (oids = false);


CREATE SEQUENCE seq_salt_events_id INCREMENT 1 MINVALUE 1 MAXVALUE 9223372036854775807 START 1 CACHE 1;

CREATE TABLE "public"."salt_events" (
   "id" bigint DEFAULT nextval('seq_salt_events_id') NOT NULL,
   "tag" character varying(255) NOT NULL,
   "data" text NOT NULL,
   "alter_time" timestamptz DEFAULT now(),
   "master_id" character varying(255) NOT NULL,
   CONSTRAINT "salt_events_id_key" UNIQUE ("id")
) WITH (oids = false);

CREATE INDEX "idx_salt_events_tag" ON "public"."salt_events" USING btree ("tag");


CREATE TABLE "public"."salt_returns" (
   "fun" character varying(50) NOT NULL,
   "jid" character varying(255) NOT NULL,
   "return" text NOT NULL,
   "full_ret" text,
   "id" character varying(255) NOT NULL,
   "success" character varying(10) NOT NULL,
   "alter_time" timestamptz DEFAULT now()
) WITH (oids = false);

CREATE INDEX "idx_salt_returns_fun" ON "public"."salt_returns" USING btree ("fun");

CREATE INDEX "idx_salt_returns_id" ON "public"."salt_returns" USING btree ("id");

CREATE INDEX "idx_salt_returns_jid" ON "public"."salt_returns" USING btree ("jid");

CREATE INDEX "idx_salt_returns_updated" ON "public"."salt_returns" USING btree ("alter_time");
