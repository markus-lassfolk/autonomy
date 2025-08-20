# Starlink API Change Response Process

Version: 2.7.1

If you receive a notification that the Starlink API schema has changed, follow these steps to quickly restore monitoring
and failover functionality:

## 1. Confirm the Change

- Check your daily Pushover notification for details on the API version change.
- Run `scripts/check_starlink_api_change.sh` manually to verify the change and view the new schema.

## 2. Review API Differences

- Compare `/root/starlink_api_schema_last.json` (previous) and `/tmp/starlink_api_schema_current.json` (current) to see
  what fields or methods have changed.
- Focus on fields used in `starlink_monitor.sh` (e.g., latency, obstruction, loss).

## 3. Update Scripts

- Edit `starlink_monitor.sh` and related scripts to match any new/renamed/removed fields.
- Use `generate_api_docs.sh` to dump the full API for reference.
- Test API calls with `grpcurl` and `jq` to confirm new field names/structure.

## 4. Validate Functionality

- Run `scripts/validate-config.sh` to check for errors.
- Use the test suite: `tests/test-suite.sh`.
- Manually trigger failover/recovery to ensure notifications and logic work.

## 5. Document and Share

- Update documentation if field names or logic change.
- Share findings with the community via GitHub Issues or PRs.

---

**Tip:** Most API changes are minor (field renames/additions). The monitoring system is designed to be resilient, but
prompt updates ensure continued reliability.
