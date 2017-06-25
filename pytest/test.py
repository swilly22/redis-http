import unittest
import redis
import requests

class SearchTestCase(unittest.TestCase):
    def testGet(self):
        r = redis.StrictRedis()
        key = '/image.jpg'
        value = 'image data'

        # Store data within redis
        r.set(key, value)

        # default port is 6380
        req = requests.get('http://127.0.0.1:6380' + key)
        self.assertEqual(req.status_code, 200)
        self.assertEqual(req.content, value)

if __name__ == '__main__':
    unittest.main()
