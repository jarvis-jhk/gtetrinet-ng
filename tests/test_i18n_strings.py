"""Every UI literal marked for translation has a German entry, and no entry is stale.

Run from the repository root: python3 -m unittest discover -s tests -p 'test_*.py'
"""
import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def calls(text, name):
    """Yields the argument text of every name(...) call, with balanced parentheses."""
    for m in re.finditer(r'\b' + name + r'\(', text):
        depth, i = 1, m.end()
        while depth and i < len(text):
            if text[i] == '"':
                i = LITERAL.match(text, i).end()
                continue
            depth += {'(': 1, ')': -1}.get(text[i], 0)
            i += 1
        yield text[m.end():i - 1]


def table():
    src = (ROOT / 'src/app/i18n.c').read_text(encoding='utf-8')
    body = src[src.index('strings[][GTN_LANG_COUNT]'):src.index('templates[][GTN_LANG_COUNT]')]
    pairs = re.findall(r'\{\s*((?:"(?:[^"\\]|\\.)*"\s*)+),\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\}', body)
    join = lambda s: ''.join(LITERAL.findall(s))
    return {join(en): join(de) for en, de in pairs}


class TranslationTable(unittest.TestCase):
    def test_marked_literals_are_translated_and_entries_used(self):
        keys = table()
        used = set()
        for path in sorted((ROOT / 'src/app').glob('*.c')):
            if path.name == 'i18n.c':
                continue
            text = path.read_text(encoding='utf-8')
            for name in ('gtn_tr', 'N_'):
                for args in calls(text, name):
                    for lit in LITERAL.findall(args):
                        with self.subTest(path=path.name, literal=lit):
                            self.assertIn(lit, keys)
                        used.add(lit)
        # Looked up at run time: the core's own error texts and the on-screen keyboard's labels.
        for path in sorted((ROOT / 'src/core').glob('*.c')):
            used.update(LITERAL.findall(path.read_text(encoding='utf-8')))
        used.update(('DEL', 'GO'))
        self.assertGreater(len(keys), 60)
        self.assertEqual(sorted(set(keys) - used), [])

    def test_screens_have_no_unmarked_sentences(self):
        """A visible English phrase in the screen code must go through gtn_tr."""
        for name in ('ui.c', 'input.c'):
            text = (ROOT / 'src/app' / name).read_text(encoding='utf-8')
            for call in ('gtn_tr', 'N_'):
                for args in calls(text, call):
                    text = text.replace(call + '(' + args + ')', '')
            text = re.sub(r'#include "[^"]*"', '', text)
            text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
            for lit in LITERAL.findall(text):
                with self.subTest(file=name, literal=lit):
                    # proper names, protocol words and key labels translated where drawn
                    self.assertTrue(lit in {'GTetrinet NG', 'TetriFast', 'DEL GO', 'Aa DEL OK', 'LB/RB'}
                                    or not re.search(r'[A-Za-z]{2}', lit), lit)


if __name__ == '__main__':
    unittest.main()
