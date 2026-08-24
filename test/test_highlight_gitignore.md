# Gitignore Highlight Test

Comments, negation, directory patterns, wildcards, and escaped characters.

```gitignore
# dependencies
node_modules/
vendor/

# environment files
.env
.env.local
.env.*.local
.env.production
.env.staging

# secrets
*.pem
*.key
*.p12
.secrets/
credentials.json
service-account*.json

# negation
!keep-me.pem
!app.env

# directory-only patterns
dist/
build/
**/logs
*~

# escaped comment
\#literal

# indented comment
   # not ignored
```
