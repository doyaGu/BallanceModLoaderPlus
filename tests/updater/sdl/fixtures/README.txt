Public TLS test fixture, intentionally not a secret.
The EC private key is the published P-256 scalar 1 (the curve generator).
The self-signed localhost certificate is valid from 2020 through 2099 and has
127.0.0.1/localhost subject alternatives. It is trusted only by the disposable
loopback HTTPS tests. Production update signing uses a different compiled key.
