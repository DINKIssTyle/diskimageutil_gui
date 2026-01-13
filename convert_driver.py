
import re

with open('/home/dinki/github/diskimageutil/Driver.h', 'r') as f:
    content = f.read()

# Extract hex values
matches = re.findall(r'0x[0-9a-fA-F]{2}', content)

# Format as Go byte slice
go_content = "package core\n\nvar AppleDriver43 = []byte{\n"
for i in range(0, len(matches), 12):
    line = ", ".join(matches[i:i+12])
    go_content += f"\t{line},\n"
go_content += "}\n"

with open('/home/dinki/github/diskimageutil/core/driver.go', 'w') as f:
    f.write(go_content)
