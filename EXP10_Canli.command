#!/bin/zsh
cd -- "${0:A:h}" || exit 1
if [[ ! -x .venv/bin/python ]]; then
  print "Python ortami bulunamadi. Proje .venv kurulumu gerekiyor."
else
  .venv/bin/python tools/exp10_live.py
fi
read "?Pencereyi kapatmak icin Enter..."
