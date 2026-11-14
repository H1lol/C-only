# Error screens

## Kinds
| Kind | Use |
|------|-----|
| Toast | Info, auto-dismiss ~4s |
| Banner | Warning |
| Modal | Error (blocking) |
| FullScreen | Critical (OOM, corrupt, GPU lost) |

## API
```cpp
report_error(err, "Import Media");
ErrorUI::instance().show_message(ErrorSeverity::Info, "Saved", "Project saved", ErrorScreenKind::Toast);
ErrorUI::instance().try_blocking(screen);
ErrorUI::instance().dismiss(id);
```

## Problems panel
`ProblemsPanel::push` also calls `ErrorUI::show`.

## Titles
Mapped per ErrorCode (Cannot Open Media, Decode Failed, …).

Actions: OK · Details · Problems · Retry (when relevant).
