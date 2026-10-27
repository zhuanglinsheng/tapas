"""Retail snapshot relations, invalid references, and symbolic payment selection."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
TAPAS = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else HERE.parents[1] / 'build/bin/tapas'


def database():
    address = dict.fromkeys(('address1', 'address2', 'city', 'state', 'country', 'zip'), '')
    return {
        'products': {
            'keyboard': {'product_id': 'keyboard', 'variants': {'key-a': {'item_id': 'key-a'}}},
            'screen': {'product_id': 'screen', 'variants': {'screen-a': {'item_id': 'screen-a'}}},
        },
        'users': {
            'alice': {'address': address.copy(), 'payment_methods': {
                'alice-card': {'source': 'credit_card'},
                'alice-gift': {'source': 'gift_card', 'balance': 10.25},
            }},
            'bob': {'address': address.copy(), 'payment_methods': {
                'bob-gift': {'source': 'gift_card', 'balance': 20.50},
            }},
        },
        'orders': {'order-a': {
            'order_id': 'order-a', 'user_id': 'alice', 'status': 'delivered',
            'address': address.copy(),
            'items': [{'product_id': 'keyboard', 'item_id': 'key-a', 'price': 9.99}],
        }},
    }


class RetailTableTests(unittest.TestCase):
    def run_tapas(self, data, body):
        with tempfile.TemporaryDirectory(prefix='tapas-retail-tables-') as directory:
            root = Path(directory)
            (root / 'data.json').write_text(json.dumps(data))
            source = ('import ' + str(HERE / 'domain.tap') + ' as domain\n' +
                      'let db = json::parse(io::read_text(' + json.dumps(str(root / 'data.json')) + '))\n' + body)
            script = root / 'test.tap'
            script.write_text(source)
            result = subprocess.run([str(TAPAS), str(script)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return result.stdout

    def test_scalar_relations_keep_owners_and_product_keys(self):
        output = self.run_tapas(database(), '''let tables = domain::relations(db)
print(json::stringify({
    'users': tables::users::rows,
    'variants': tables::variants::rows,
    'methods': tables::payment_methods::rows,
}))
''')
        rows = json.loads(output)
        self.assertEqual({tuple(r) for r in rows['variants']}, {('keyboard', 'key-a'), ('screen', 'screen-a')})
        self.assertEqual({tuple(r) for r in rows['users']}, {('alice',), ('bob',)})
        self.assertEqual({tuple(r) for r in rows['methods']}, {
            ('alice', 'alice-card', 'credit_card'), ('alice', 'alice-gift', 'gift_card'),
            ('bob', 'bob-gift', 'gift_card'),
        })

    def test_invalid_references_and_domains_are_not_filtered_away(self):
        data = database()
        flags = json.loads(self.run_tapas(data, 'print(json::stringify(domain::facts(db)))\n'))
        self.assertTrue(flags['order_references'])
        for label in ('cross-product', 'unknown-user', 'status', 'payment-source'):
            changed = copy.deepcopy(data)
            if label == 'cross-product':
                changed['orders']['order-a']['items'][0]['product_id'] = 'screen'
                field = 'order_references'
            elif label == 'unknown-user':
                changed['orders']['order-a']['user_id'] = 'unknown'
                field = 'order_references'
            elif label == 'status':
                changed['orders']['order-a']['status'] = 'unknown'
                field = 'statuses'
            else:
                changed['users']['alice']['payment_methods']['alice-card']['source'] = 'unknown'
                field = 'payment_sources'
            flags = json.loads(self.run_tapas(changed, 'print(json::stringify(domain::facts(db)))\n'))
            self.assertFalse(flags[field], label)

    def test_solver_selects_a_payment_method_from_the_same_user_row(self):
        output = self.run_tapas(database(), '''var methods = domain::relations(db)::payment_methods
let UserId = types::enum('alice', 'bob')
let MethodId = types::enum('alice-card', 'alice-gift', 'bob-gift')
let Source = types::enum('gift_card', 'credit_card', 'paypal')
let PaymentChoice = rule (user_id: UserId, method_id: MethodId, source: Source) {
    [user_id, method_id, source] in methods
    user_id == 'alice'
    source == 'gift_card'
}
let answer = solve::hold(PaymentChoice)
assert(rule { answer::status == 'sat' })
assert(answer::witness)
let chosen = arguments(answer::witness)
assert(rule { chosen[0] == 'alice'; chosen[1] == 'alice-gift'; chosen[2] == 'gift_card' })
print('payment table: ok')
''')
        self.assertEqual(output, 'payment table: ok\n')


if __name__ == '__main__':
    unittest.main()
