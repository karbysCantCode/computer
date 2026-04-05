from time import sleep
import logging

from AppKit import NSWorkspace

logger = logging.getLogger('focusstealer')

logger.setLevel(logging.INFO)
stream_handler = logging.StreamHandler()
stream_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)-8s %(name)s: %(message)s", "%Y-%m-%d %H:%M:%S"))
logger.addHandler(stream_handler)

workspace = NSWorkspace.sharedWorkspace()
active_app = None
while True:
    prev_app, active_app = active_app, workspace.activeApplication()
    if prev_app != active_app:
        logger.info('Focus changed to: %s (%s)', active_app['NSApplicationName'], active_app['NSApplicationPath'])
    sleep(0.5)