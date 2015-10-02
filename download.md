---
title: cxNES Download
layout: default
---

{% for release in site.github.releases %}
 {% if release.draft %}
    {% continue %}
  {% endif %}
## [{{ release.name }}]({{ release.html_url }})  

  {{ release.body }}

{% for asset in release.assets %}
{% assign: sz = asset.size | times: 100.0 | divided_by: 1048576.0 | plus: 0.5 %}
{% assign: sz = sz %}
{% capture sz %}{{ sz }}{% endcapture %}
[{% if asset.label %}{{ asset.label }}{% else %}{{ asset.name }}{% endif %}]({{ asset.browser_download_url }}) {{ sz }} MiB  
{% endfor %}[Source code (.zip)]({{ release.zipball_url }})  
[Source code (.tar.gz)]({{ release.tarball_url }})
{% endfor %}


