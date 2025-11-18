class Command:
    def __init__(self, type, args=None):
        self.type = type
        self.args = args or {}

def parse_command(text: str) -> Command:
    tokens = text.strip().split()

    if not tokens:
        return Command("EMPTY")

    cmd = tokens[0].lower()

    if cmd == "submit" and len(tokens) == 2:
        return Command("SUBMIT", {"file": tokens[1]})

    if cmd == "status" and len(tokens) == 2:
        return Command("STATUS", {"jobID": tokens[1]})

    if cmd == "logs" and len(tokens) == 2:
        return Command("LOGS", {"jobID": tokens[1]})

    return Command("RAW", {"text": text})

