## Config file for sesman.

[sesman]
# Display a warning when the session is recorded.
record_warning=boolean(default=True)

# Case-sensitivity on selector filters.
selector_filters_case_sensitive=boolean(default=False)

# Allow going back to selector
allow_back_to_selector = boolean(default=True)

# On interactive subnet, Kerberos authentication requires to keep the fqdn after resolution.
# This is an internal option that allows to go back to previous behavior.
#_advanced
keep_interactive_fqdn = boolean(default=True)

# On transparent mode, Kerberos authentication requires to keep the fqdn after resolution.
# This is an internal option that allows to go back to previous behavior.
#_advanced
keep_context_fqdn = boolean(default=True)

# Ignore username when authenticated by Kerberos
# when authenticated by Kerberos, only use User Principal Name.
# This could be useful when the RDP client provide the pre-2000 logon format
# as username and it is totally different from User Principal
#_advanced
kerberos_ignore_username=boolean(default=True)

# Mode passthrough.
auth_mode_passthrough=boolean(default=False)

# Default login (for passthrough mode, disabled if empty).
default_login=string(default='')

# Debug Logs
#_advanced
debug=boolean(default=False)
