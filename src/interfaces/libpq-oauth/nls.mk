# src/interfaces/libpq-oauth/nls.mk
CATALOG_NAME     = libpq-oauth
GETTEXT_FILES    = oauth-curl.c \
                   oauth-utils.c
GETTEXT_TRIGGERS = actx_error:2 \
                   libpq_append_conn_error:2 \
                   libpq_append_error:2 \
                   libpq_gettext \
                   libpq_ngettext:1,2
GETTEXT_FLAGS    = actx_error:2:c-format \
                   libpq_append_conn_error:2:c-format \
                   libpq_append_error:2:c-format \
                   libpq_gettext:1:pass-c-format \
                   libpq_ngettext:1:pass-c-format \
                   libpq_ngettext:2:pass-c-format
