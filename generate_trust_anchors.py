import argparse
import base64
import re
from pathlib import Path

from cryptography import x509
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa


def clean_pem_to_der(pem_bytes):
    match = re.search(
        rb"-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----",
        pem_bytes,
        re.S,
    )
    if not match:
        raise ValueError("No encontre bloque BEGIN/END CERTIFICATE")

    b64 = re.sub(rb"[^A-Za-z0-9+/=]", b"", match.group(1))
    der = base64.b64decode(b64)

    if not der or der[0] != 0x30:
        raise ValueError("DER invalido: no inicia con SEQUENCE")

    length_byte = der[1]
    if length_byte < 0x80:
        total = 2 + length_byte
    else:
        length_size = length_byte & 0x7F
        length = int.from_bytes(der[2 : 2 + length_size], "big")
        total = 2 + length_size + length

    return der[:total], len(der) - total


def c_array(name, data):
    lines = [f"static const unsigned char {name}[] = {{"]

    for i in range(0, len(data), 12):
        chunk = data[i : i + 12]
        values = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"  {values},")

    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Convierte un certificado PEM RSA a trust_anchors.h para SSLClient/BearSSL."
    )
    parser.add_argument("input", help="Certificado PEM de entrada")
    parser.add_argument(
        "-o",
        "--output",
        default="trust_anchors.h",
        help="Archivo .h de salida",
    )
    args = parser.parse_args()

    pem_path = Path(args.input)
    out_path = Path(args.output)

    der, extra = clean_pem_to_der(pem_path.read_bytes())

    if extra > 0:
        print(f"AVISO: recortando {extra} bytes extra del certificado")

    cert = x509.load_der_x509_certificate(der)
    print("Certificado:", cert.subject.rfc4514_string())

    pub = cert.public_key()
    if not isinstance(pub, rsa.RSAPublicKey):
        raise TypeError("Este certificado no es RSA. Para MQTT usa R13 o ISRG Root X1, no E7.")

    dn = cert.subject.public_bytes(serialization.Encoding.DER)
    nums = pub.public_numbers()

    n_bytes = nums.n.to_bytes((nums.n.bit_length() + 7) // 8, "big")
    e_bytes = nums.e.to_bytes((nums.e.bit_length() + 7) // 8, "big")

    content = ""
    content += "// trust_anchors.h - Auto-generado\n"
    content += "#ifndef TRUST_ANCHORS_H\n"
    content += "#define TRUST_ANCHORS_H\n\n"
    content += "#include <SSLClient.h>\n\n"
    content += c_array("TA_DN", dn)
    content += c_array("TA_RSA_N", n_bytes)
    content += c_array("TA_RSA_E", e_bytes)

    content += "static const br_x509_trust_anchor TAs[] = {\n"
    content += "  {\n"
    content += "    { (unsigned char *)TA_DN, sizeof(TA_DN) },\n"
    content += "    BR_X509_TA_CA,\n"
    content += "    {\n"
    content += "      BR_KEYTYPE_RSA,\n"
    content += "      { .rsa = {\n"
    content += "        (unsigned char *)TA_RSA_N,\n"
    content += "        sizeof(TA_RSA_N),\n"
    content += "        (unsigned char *)TA_RSA_E,\n"
    content += "        sizeof(TA_RSA_E)\n"
    content += "      } }\n"
    content += "    }\n"
    content += "  }\n"
    content += "};\n\n"
    content += "#define TAs_NUM (sizeof(TAs) / sizeof(TAs[0]))\n\n"
    content += "#endif\n"

    out_path.write_text(content, encoding="utf-8")

    print(f"{out_path} generado OK")
    print(f"DN: {len(dn)} bytes")
    print(f"RSA: {nums.n.bit_length()} bits")
    print(f"e: {nums.e}")


if __name__ == "__main__":
    main()