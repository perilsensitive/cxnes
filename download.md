---
title: cxNES Download
layout: default
---

{% for release in site.github.releases %}
 {% if release.draft %}
    {% continue %}
  {% endif %}
<a class="release_link" href="{{ release.html_url }}">{{ release.name }}</a> {{ release.created_at }}  

  {{ release.body }}

{% for asset in release.assets %}{% if asset.state != "uploaded" %}{% continue %}{% endif %}{% assign: sz = asset.size | times: 100.0 | divided_by: 1048576.0 | plus: 0.5 %}{% assign: sz = sz %}{% capture sz %}{{ sz }}{% endcapture %}{% assign: sz = sz | split: "."  | first %}[{% if asset.label %}{{ asset.label }}{% else %}{{ asset.name }}{% endif %}]({{ asset.browser_download_url }}) {{ sz | divided_by: 100.0 }} MB  
{% endfor %}[Source code (.zip)]({{ release.zipball_url }})  
[Source code (.tar.gz)]({{ release.tarball_url }})
{% endfor %}


