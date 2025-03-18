# split_log.py

(requiere python 3.11)

Usage and help with `split_log.py -h`.

```sh
./split_log.py syslog wabproxy.log others.log...
# or
grep my_pattern file.log... | ./split_log.py
```

Create log in another directory with `-p`/`--prefix`:

```sh
./split_log.py -p my_ouput_dir/ syslog wabproxy.log
```

Trailing `/` is important for directory, because output filename is concatenated without separator.


# rdpproxy_color.awk

(require gawk)

```sh
./rdpproxy_color.awk file.log...
# or
grep my_pattern file.log... | ./rdpproxy_color.awk
```
