# distributed-autocomplete
A distributed in-memory datastore serving token-prefix autocomplete over per-tenant corpora (100K–1M entries each) for a multi-tenant BI analytics platform. Read-only query path. Index content is derived from tenant schema seeds plus an accumulating history of past BI queries.
