# Screenshots — Release 1.4.0

The screenshots for this release are generated with `shot.sh`, which builds a
debug exe (`-DAZ_DEBUG_BUILD`) and renders each screen under **Wine + Xvfb**.
Wine is not present in the headless CI sandbox used for this build, so the PNGs
must be produced on a Wine-equipped machine (or on Windows directly). Run:

```bash
./shot.sh reception   docs/shots/1.4.0/01_reception.png
./shot.sh settings    docs/shots/1.4.0/02_reception_settings.png
./shot.sh manage      docs/shots/1.4.0/03_management_settings.png
./shot.sh designer    docs/shots/1.4.0/04_print_designer.png
./shot.sh restore     docs/shots/1.4.0/05_restore_design.png
./shot.sh sections    docs/shots/1.4.0/06_sections_registry.png
./shot.sh profreq     docs/shots/1.4.0/07_profile_requests.png
./shot.sh backuplog   docs/shots/1.4.0/08_backup_log_viewer.png
./shot.sh home        docs/shots/1.4.0/09_home.png
./shot.sh manage      docs/shots/1.4.0/10_management_dashboard.png
```

> NOTE: `docs/shots/` is listed in `.gitignore`, so the rendered PNGs stay out
> of version control by design. This manifest documents exactly which views to
> capture for the 1.4.0 release (§15).

## Capture checklist (10 views)

| # | Screen        | What to show                                            |
|---|---------------|---------------------------------------------------------|
| 1 | Reception     | Form with reset button + collapsed header on scroll     |
| 2 | Reception settings | 7-item nav (profile/theme/printer/…)                |
| 3 | Management settings | 16-item nav                                       |
| 4 | Print designer | Section picker → canvas with a template loaded         |
| 5 | Restore design | `.aztpl` import window                                  |
| 6 | Sections      | Departments registry list                               |
| 7 | Profile requests | Admin inbox with approve/reject                      |
| 8 | Backup-log viewer | Filter chips همه/موفق/ناموفق + details pane          |
| 9 | Home          | Login / landing screen                                   |
| 10| Management    | Dashboard / patients page                               |
